#include "device_scanner.h"
#include "discovery_protocols.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <cerrno>
#include <algorithm>
namespace WifiInspector {
namespace {
uint32_t number(const IPAddress &a){return uint32_t(a[0])<<24|uint32_t(a[1])<<16|uint32_t(a[2])<<8|a[3];}
IPAddress address(uint32_t n){return IPAddress(n>>24,n>>16,n>>8,n);}
class Socket {
public:int fd=-1;~Socket(){close();}void close(){if(fd>=0){::close(fd);fd=-1;}}
};
void arpRead(netif *iface,const Network &n,ScanResult &r){
    LOCK_TCPIP_CORE();
    for(size_t i=0;i<ARP_TABLE_SIZE;++i){
        ip4_addr_t *ip=nullptr;eth_addr *mac=nullptr;netif *entryIf=nullptr;
        if(!etharp_get_entry(i,&ip,&entryIf,&mac)||entryIf!=iface)continue;
        uint32_t host=ntohl(ip->addr);
        if(host==n.local||!n.contains(host)||((host<r.first||host>r.last)&&host!=n.gateway&&!r.find(host)))continue;
        Mac m{};memcpy(m.data(),mac->addr,6);if(!validMac(m))continue;
        Device *d=r.add(host,m);if(d)d->evidence|=Arp;
    }
    UNLOCK_TCPIP_CORE();
}
void arpRequest(netif *iface,uint32_t ip){
    ip4_addr_t target{htonl(ip)};LOCK_TCPIP_CORE();etharp_request(iface,&target);UNLOCK_TCPIP_CORE();
}
}
String ipText(uint32_t n){return address(n).toString();}
String macText(const Mac &m){char s[18];snprintf(s,sizeof(s),"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);return s;}
bool currentNetwork(Network &n){
    if(!WiFi.isConnected()||!(WiFi.getMode()&WIFI_MODE_STA))return false;
    n.local=number(WiFi.localIP());n.mask=number(WiFi.subnetMask());n.gateway=number(WiFi.gatewayIP());n.dns=number(WiFi.dnsIP());
    const uint8_t *bssid=WiFi.BSSID();if(!bssid)return false;memcpy(n.bssid.data(),bssid,6);
    WiFi.SSID().toCharArray(n.ssid,sizeof(n.ssid));return n.valid();
}
bool DeviceScanner::run(const Network &n,const ScanSettings &settings,ScanResult &r,const ProgressCallback &progress){
    for(auto &device:r.devices)device=Device{};
    r.count=0;r.complete=false;r.limited=false;r.overflow=false;r.first=0;r.last=0;r.elapsed=0;
    uint32_t started=millis(),checked=started-250;bool cancelled=false;
    auto alive=[&](const char *phase,uint8_t percent,uint32_t ip){
        if(uint32_t(millis()-started)>90000){cancelled=true;return false;}
        if(uint32_t(millis()-checked)>=250){
            Network now;checked=millis();
            if(!currentNetwork(now)||!n.same(now)||now.local!=n.local||now.gateway!=n.gateway){cancelled=true;return false;}
        }
        if(!progress({phase,percent,ip,r.count})){cancelled=true;return false;}delay(5);return true;
    };
    auto wait=[&](uint32_t ms,const char *phase,uint8_t percent,uint32_t ip){uint32_t t=millis();do{if(!alive(phase,percent,ip))return false;}while(uint32_t(millis()-t)<ms);return true;};
    esp_netif_t *espif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    netif *iface=espif?static_cast<netif *>(esp_netif_get_netif_impl(espif)):nullptr;
    if(!iface||!n.valid())return false;
    r.first=settings.start?settings.start:((prefixLength(n.mask)<24)?((n.local&0xffffff00u)+1):n.address()+1);
    if(!n.contains(r.first))return false;
    r.last=uint32_t(std::min(uint64_t(n.broadcast()-1),uint64_t(r.first)+253));
    r.limited=r.first!=n.address()+1||r.last!=n.broadcast()-1;
    if(n.contains(n.gateway))arpRequest(iface,n.gateway);
    for(uint32_t ip=r.first;ip<=r.last;++ip){
        uint8_t percent=uint8_t((ip-r.first)*35/(r.last-r.first+1));
        if(!alive("Descoberta ARP",percent,ip))break;
        if(ip!=n.local)arpRequest(iface,ip);
        if(!wait(25,"Descoberta ARP",percent,ip))break;
        arpRead(iface,n,r);
    }
    if(!cancelled){wait(300,"Aguardando ARP",35,n.gateway);arpRead(iface,n,r);}
    // Own ephemeral UDP sockets; do not start/stop the shared WebUI mDNS responder.
    WiFiUDP udp;
    if(!cancelled&&settings.ssdp&&udp.begin(uint16_t(0))){
        static const char query[]="M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: ssdp:all\r\n\r\n";
        udp.beginPacket(IPAddress(239,255,255,250),1900);udp.write(reinterpret_cast<const uint8_t *>(query),sizeof(query)-1);udp.endPacket();
        uint32_t t=millis();uint8_t packet[1500];
        while(uint32_t(millis()-t)<1300&&alive("Servicos SSDP",38,0)){
            int bytes=udp.parsePacket();if(bytes<=0)continue;
            uint32_t ip=number(udp.remoteIP());
            if(bytes>=int(sizeof(packet))||!n.contains(ip)||ip==n.local){udp.clear();continue;}
            int got=udp.read(packet,sizeof(packet)-1);if(got<=0)continue;packet[got]=0;char service[80]{};
            if(parseSsdp(reinterpret_cast<char *>(packet),got,service,sizeof(service))){Device *d=r.add(ip);if(d){memcpy(d->advertised,service,sizeof(service));d->evidence|=Ssdp;arpRequest(iface,ip);}}
        }
        udp.stop();arpRead(iface,n,r);
    }
    const size_t total=r.count;
    for(size_t i=0;i<total&&!cancelled;++i){
        Device &d=r.devices[i];uint8_t percent=40+i*59/std::max(size_t(1),total);
        if(!alive("Identificando",percent,d.ip))break;
        if(settings.names&&udp.begin(uint16_t(0))){
            char reverse[64];reverseName(d.ip,reverse,sizeof(reverse));uint8_t packet[1500];
            for(int mode=0;mode<2&&!cancelled;++mode){
                bool mdns=mode==1;uint32_t dns=n.dns;
                // Never send local reverse DNS queries to a public resolver.
                if(!mdns&&!n.contains(dns))continue;
                uint16_t id=mdns?0:uint16_t(esp_random());size_t length=dnsQuestion(packet,sizeof(packet),reverse,id,mdns);
                udp.beginPacket(mdns?IPAddress(224,0,0,251):address(dns),mdns?5353:53);udp.write(packet,length);udp.endPacket();
                uint32_t t=millis();
                while(uint32_t(millis()-t)<200&&alive(mdns?"Nome mDNS":"Nome DNS local",percent,d.ip)){
                    int bytes=udp.parsePacket();if(bytes<=0)continue;
                    uint32_t from=number(udp.remoteIP());uint16_t port=udp.remotePort();
                    if(bytes>int(sizeof(packet))||port!=(mdns?5353:53)||from!=(mdns?d.ip:dns)){udp.clear();continue;}
                    int got=udp.read(packet,sizeof(packet));if(got<=0)continue;NameInfo names;
                    if(!parseDns(packet,got,d.ip,reverse,id,mdns,names))continue;
                    if(names.hostname[0]){memcpy(d.hostname,names.hostname,sizeof(d.hostname));d.evidence|=Dns;}
                    if(names.mdns[0]){memcpy(d.mdns,names.mdns,sizeof(d.mdns));d.evidence|=Mdns;}
                    if(names.service[0])memcpy(d.advertised,names.service,sizeof(d.advertised));
                }
            }
            udp.stop();
        }
        if(settings.tcp)for(size_t p=0;p<PORT_COUNT&&!cancelled;++p){
            if(!alive("Portas TCP",percent,d.ip))break;
            Socket sock;sock.fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(sock.fd<0)continue;
            if(fcntl(sock.fd,F_SETFL,O_NONBLOCK)<0)continue;
            sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(PORTS[p]);target.sin_addr.s_addr=htonl(d.ip);
            int status=connect(sock.fd,reinterpret_cast<sockaddr *>(&target),sizeof(target));
            bool open=status==0;
            if(status<0&&errno==ECONNREFUSED)d.evidence|=Tcp;
            if(status<0&&errno==EINPROGRESS){
                uint32_t t=millis();
                while(uint32_t(millis()-t)<settings.timeoutMs&&alive("Portas TCP",percent,d.ip)){
                    fd_set writes;FD_ZERO(&writes);FD_SET(sock.fd,&writes);timeval tv{};
                    if(select(sock.fd+1,nullptr,&writes,nullptr,&tv)>0){int error=0;socklen_t len=sizeof(error);if(getsockopt(sock.fd,SOL_SOCKET,SO_ERROR,&error,&len)==0){open=error==0;if(error==ECONNREFUSED)d.evidence|=Tcp;}break;}
                }
            }
            if(open){d.ports|=uint16_t(1u<<p);d.evidence|=Tcp;}
            sock.close();
        }
    }
    udp.stop();r.elapsed=millis()-started;r.complete=!cancelled&&!r.overflow;r.limited|=r.overflow;
    if(!cancelled)progress({"Concluido",100,0,r.count});
    return !cancelled;
}
}
