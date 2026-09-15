#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

namespace WifiInspector {
constexpr size_t MAX_DEVICES = 64, MAX_HISTORY = 96;
constexpr uint16_t PORTS[] = {22,53,80,139,443,445,515,554,631,1883,8008,8080,8443,9100};
constexpr const char *SERVICES[] = {"SSH","DNS TCP","HTTP","NetBIOS TCP","HTTPS","SMB","LPD","RTSP","IPP","MQTT","Google/Cast","HTTP alternativo","HTTPS alternativo","RAW Printing"};
constexpr size_t PORT_COUNT = sizeof(PORTS)/sizeof(PORTS[0]);
using Mac = std::array<uint8_t,6>;
constexpr bool validMac(const Mac &m) {
    return !(m[0]&1) && (m[0]||m[1]||m[2]||m[3]||m[4]||m[5]);
}
constexpr bool privateMac(const Mac &m) { return validMac(m) && (m[0]&2); }
constexpr uint8_t prefixLength(uint32_t mask) {
    uint8_t bits=0; bool zero=false;
    for (int i=31;i>=0;--i) {
        if (mask & (uint32_t(1)<<i)) { if(zero) return 0; ++bits; } else zero=true;
    }
    return bits;
}
constexpr bool unicast(uint32_t ip) { return (ip>>24)>0 && (ip>>24)<224 && (ip>>24)!=127; }
struct Network {
    uint32_t local=0,mask=0,gateway=0,dns=0; // canonical network-order numeric value
    Mac bssid{};
    char ssid[33]{};
    uint32_t address() const { return local & mask; }
    uint32_t broadcast() const { return address() | ~mask; }
    bool contains(uint32_t ip) const { return unicast(ip) && (ip&mask)==address() && ip>address() && ip<broadcast(); }
    bool valid() const { const auto n=prefixLength(mask); return n && n<=30 && contains(local) && validMac(bssid); }
    bool same(const Network &n) const { return mask==n.mask && address()==n.address() && bssid==n.bssid && !strcmp(ssid,n.ssid); }
};
enum Evidence : uint8_t { Arp=1, Tcp=2, Mdns=4, Ssdp=8, Dns=16 };
struct Device {
    Mac mac{};
    uint32_t ip=0,firstSeen=0,lastSeen=0,sightings=0;
    uint16_t ports=0;
    uint8_t evidence=0;
    bool known=false,present=false,previous=false,isNew=false;
    char name[48]{},note[96]{},hostname[64]{},mdns[64]{},advertised[80]{};
};
struct ScanResult {
    std::array<Device,MAX_DEVICES> devices{};
    size_t count=0;
    bool complete=false,limited=false,overflow=false;
    uint32_t first=0,last=0,elapsed=0;
    Device *find(uint32_t ip) {
        for(size_t i=0;i<count;++i) if(devices[i].ip==ip)return &devices[i];
        return nullptr;
    }
    Device *add(uint32_t ip,const Mac &mac={}) {
        Device *d=find(ip);
        if(!d) {
            if(count==MAX_DEVICES){overflow=true;return nullptr;}
            d=&devices[count++]; d->ip=ip; d->present=true;
        }
        if(validMac(mac))d->mac=mac;
        return d;
    }
};
struct Database {
    Network network{};
    std::array<Device,MAX_HISTORY> devices{};
    size_t count=0;
    bool dirty=false,full=false;
    Device *find(const Mac &mac) {
        if(!validMac(mac))return nullptr;
        for(size_t i=0;i<count;++i) if(devices[i].mac==mac)return &devices[i];
        return nullptr;
    }
    void merge(ScanResult &scan,uint32_t epoch) {
        full=false;
        for(size_t i=0;i<count;++i) {
            devices[i].previous=devices[i].present;
            if(scan.complete && ((devices[i].ip>=scan.first&&devices[i].ip<=scan.last)||devices[i].ip==network.gateway))devices[i].present=false;
            devices[i].isNew=false;
        }
        for(size_t i=0;i<scan.count;++i) {
            auto &seen=scan.devices[i];
            if(!validMac(seen.mac))continue; // IP-only observations never become persistent identities.
            Device *saved=find(seen.mac);
            bool fresh=!saved;
            seen.isNew=fresh;
            seen.present=true;
            if(!saved) {
                if(count==MAX_HISTORY){full=true;continue;}
                saved=&devices[count++];saved->mac=seen.mac;saved->firstSeen=epoch;
            }
            // Preserve manual labels and trust through DHCP address changes.
            seen.known=saved->known; memcpy(seen.name,saved->name,sizeof(seen.name));
            memcpy(seen.note,saved->note,sizeof(seen.note));
            // Missing replies do not erase the last name/service that was actually observed.
            if(!seen.hostname[0])memcpy(seen.hostname,saved->hostname,sizeof(seen.hostname));
            if(!seen.mdns[0])memcpy(seen.mdns,saved->mdns,sizeof(seen.mdns));
            if(!seen.advertised[0])memcpy(seen.advertised,saved->advertised,sizeof(seen.advertised));
            seen.firstSeen=saved->firstSeen;seen.lastSeen=epoch;
            seen.sightings=saved->sightings==UINT32_MAX?UINT32_MAX:saved->sightings+1;
            seen.previous=saved->previous;seen.present=true;seen.isNew=fresh;
            *saved=seen;dirty=true;
        }
    }
};
static_assert(sizeof(Database)<=37*1024,"Inspector history memory budget exceeded");
static_assert(sizeof(ScanResult)<=25*1024,"Inspector scan memory budget exceeded");
inline bool hasPort(const Device &d,uint16_t port) {
    for(size_t i=0;i<PORT_COUNT;++i)if(PORTS[i]==port)return d.ports&(1u<<i);
    return false;
}
inline void cleanText(char *out,size_t size,const char *in) {
    if(!size)return;
    size_t n=0;
    while(in && *in && n+1<size) { unsigned char c=*in++;out[n++]=(c>=32 && c<=126)?char(c):'?'; }
    out[n]=0;
}
}
