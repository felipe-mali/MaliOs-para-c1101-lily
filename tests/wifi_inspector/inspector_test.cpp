#include "../../src/modules/wifi/inspector/device_database.h"
#include "../../src/modules/wifi/inspector/device_classifier.h"
#include "../../src/modules/wifi/inspector/discovery_protocols.h"
#include "../../src/modules/wifi/inspector/oui_database.h"
#include "../../src/modules/wifi/inspector/router_manager.h"
#include <cassert>
#include <cstdio>
#include <random>
using namespace WifiInspector;
Network network(){Network n;n.local=0xc0a80f14;n.mask=0xffffff00;n.gateway=0xc0a80f01;n.dns=n.gateway;n.bssid={0,1,2,3,4,5};strcpy(n.ssid,"Bancada");return n;}
ScanResult observation(uint32_t ip=0xc0a80f2c){ScanResult s;s.first=0xc0a80f01;s.last=0xc0a80ffe;s.complete=true;s.add(ip,{0,0x17,0x87,1,2,3});return s;}
void databaseTests(){
    Database db;db.network=network();auto scan=observation();db.merge(scan,1800000000);
    assert(db.count==1&&db.devices[0].isNew&&db.devices[0].sightings==1);
    db.devices[0].known=true;strcpy(db.devices[0].name,"Impressora Financeiro");strcpy(db.devices[0].note,"Patrimonio 123");
    strcpy(db.devices[0].hostname,"printer.local");
    scan=observation(0xc0a80f39);db.merge(scan,1800000010);
    assert(db.count==1&&db.devices[0].known&&!db.devices[0].isNew&&db.devices[0].ip==0xc0a80f39);
    assert(!strcmp(scan.devices[0].name,"Impressora Financeiro")&&db.devices[0].sightings==2);
    assert(!strcmp(scan.devices[0].hostname,"printer.local"));
    ScanResult partial;partial.first=scan.first;partial.last=scan.last;db.merge(partial,1800000020);assert(db.devices[0].present);
    partial.complete=true;partial.first=0xc0a80f80;partial.last=0xc0a80ffe;db.merge(partial,1800000030);assert(db.devices[0].present);
    partial.first=0xc0a80f01;db.merge(partial,1800000040);assert(!db.devices[0].present&&db.devices[0].previous);
    db.devices[0].sightings=UINT32_MAX;scan=observation();db.merge(scan,0);assert(db.devices[0].sightings==UINT32_MAX&&db.devices[0].lastSeen==0);
    ScanResult noMac;noMac.add(0xc0a80f66);db.merge(noMac,0);assert(db.count==1);
    db.count=MAX_HISTORY;scan=observation();scan.devices[0].mac[5]=99;db.merge(scan,0);assert(db.count==MAX_HISTORY&&db.full&&scan.devices[0].isNew);
    ScanResult bounded;for(unsigned i=0;i<100;++i)bounded.add(0xc0a80f01+i);assert(bounded.count==MAX_DEVICES&&bounded.overflow);
}
void storageTests(){
    FS fs;Database db;db.network=network();DeviceStore store(fs,db.network);assert(store.load(db));auto scan=observation();db.merge(scan,1800000000);
    db.devices[0].known=true;strcpy(db.devices[0].name,"Minha impressora");assert(store.save(db)&&!db.dirty);
    Database loaded;loaded.network=network();assert(store.load(loaded)&&loaded.count==1&&loaded.devices[0].known);
    assert(!strcmp(loaded.devices[0].name,"Minha impressora"));
    auto original=fs.memory->files[store.path()]->bytes;
    for(size_t i:{size_t(0),size_t(5),size_t(70),size_t(300),original.size()-1}){
        fs.memory->files[store.path()]->bytes=original;fs.memory->files[store.path()]->bytes[i]^=0x55;
        loaded.count=0;assert(!store.load(loaded)&&loaded.count==0);
    }
    fs.memory->files[store.path()]->bytes=original;
    db.dirty=true;fs.memory->failWrites=true;assert(!store.save(db)&&db.dirty);assert(fs.memory->files[store.path()]->bytes==original);fs.memory->failWrites=false;
    strcpy(db.devices[0].name,"Versao nova");fs.memory->failCommit=true;assert(!store.save(db));assert(fs.memory->files[store.path()]->bytes==original);fs.memory->failCommit=false;
    assert(fs.rename(store.path(),store.path()+".bak"));loaded.count=0;assert(store.load(loaded)&&loaded.count==1&&fs.exists(store.path()));
    assert(store.save(db));loaded.count=0;assert(store.load(loaded)&&!strcmp(loaded.devices[0].name,"Versao nova"));
    // Same file key but another SSID is not silently trusted.
    strcpy(loaded.network.ssid,"Outra rede");assert(!store.load(loaded));
    assert(store.erase()&&!fs.exists(store.path())&&!fs.exists(store.path()+".bak"));
}
void protocolTests(){
    char out[256]{};size_t at=0;
    const uint8_t name[]={3,'f','o','o',5,'l','o','c','a','l',0,0xc0,0};
    assert(dnsName(name,sizeof(name),at,out,sizeof(out))&&!strcmp(out,"foo.local")&&at==11);
    assert(dnsName(name,sizeof(name),at,out,sizeof(out))&&!strcmp(out,"foo.local")&&at==13);
    const uint8_t cycle[]={0xc0,0};at=0;assert(!dnsName(cycle,sizeof(cycle),at,out,sizeof(out)));
    const uint8_t truncated[]={7,'a'};at=0;assert(!dnsName(truncated,sizeof(truncated),at,out,sizeof(out)));
    uint8_t packet[1500]{};char reverse[64];reverseName(0xc0a80f2c,reverse,sizeof(reverse));
    size_t n=dnsQuestion(packet,sizeof(packet),reverse,0x1234,false);assert(n>12&&read16(packet)==0x1234&&packet[5]==1);
    packet[2]=0x81;packet[3]=0x80;packet[7]=1;
    const uint8_t answer[]={0xc0,0x0c,0,12,0,1,0,0,0,60,0,11,3,'f','o','o',5,'l','o','c','a','l',0};
    memcpy(packet+n,answer,sizeof(answer));n+=sizeof(answer);NameInfo info;
    assert(parseDns(packet,n,0xc0a80f2c,reverse,0x1234,false,info)&&!strcmp(info.hostname,"foo.local"));
    assert(!parseDns(packet,n,0xc0a80f2c,reverse,0x4321,false,info));
    for(size_t size=0;size<n;++size)assert(!parseDns(packet,size,0xc0a80f2c,reverse,0x1234,false,info));
    char ssdp[80];const char *reply="HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\nLOCATION: http://untrusted.invalid/\r\n\r\n";
    assert(parseSsdp(reply,strlen(reply),ssdp,sizeof(ssdp))&&strstr(ssdp,"MediaRenderer"));
    assert(!parseSsdp(reply,8,ssdp,sizeof(ssdp)));
    std::mt19937 random(42);
    for(int trial=0;trial<20000;++trial){size_t size=random()%1501;for(size_t i=0;i<size;++i)packet[i]=uint8_t(random());at=0;dnsName(packet,size,at,out,sizeof(out));parseDns(packet,size,0xc0a80f2c,reverse,0,true,info);parseSsdp(reinterpret_cast<char *>(packet),size,ssdp,sizeof(ssdp));}
}
void classifierTests(){
    Device d;d.mac={0,0x17,0x87,1,2,3};d.ip=0xc0a80f2c;assert(!strcmp(manufacturer(d.mac),"Brother"));
    d.ports=1<<8;assert(!strcmp(classify(d,0),"Provavel impressora"));d.ports=1<<7;assert(!strcmp(classify(d,0),"Possivel camera/DVR"));
    assert(!strcmp(classify(d,d.ip),"Gateway da conexao"));d.mac[0]=2;assert(privateMac(d.mac)&&!strcmp(manufacturer(d.mac),"Desconhecido"));
    GenericRouter router;assert(!router.isManagementSupported()&&router.authenticate()==ManagementResult::Unsupported&&router.disconnectClient(d.mac)==ManagementResult::Unsupported);router.logout();
}
int main(){
    static_assert(prefixLength(0xffffff00)==24&&prefixLength(0xff00ff00)==0);
    assert(network().valid()&&!network().contains(0xc0a80f00)&&!network().contains(0xc0a80fff));
    databaseTests();storageTests();protocolTests();classifierTests();
    printf("PASS: identity/DHCP, partial scans, bounds, storage roundtrip/corruption/write/rename/recovery, DNS compression/truncation/fuzz, classification and generic router\n");
    printf("Sizes: Device=%zu, Database=%zu, ScanResult=%zu bytes\n",sizeof(Device),sizeof(Database),sizeof(ScanResult));
}
