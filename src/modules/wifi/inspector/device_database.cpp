#include "device_database.h"
#include <cstdio>
namespace WifiInspector {
namespace {
constexpr size_t HEADER_SIZE=65, RECORD_SIZE=378;
constexpr size_t MAX_NETWORKS=8;
uint32_t crcByte(uint32_t crc,uint8_t byte) {
    crc^=byte;for(int n=0;n<8;++n)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));return crc;
}
struct Bytes {
    uint8_t *p;size_t offset=0;
    void put(uint32_t v,size_t n){for(size_t i=0;i<n;++i)p[offset++]=uint8_t(v>>(i*8));}
    uint32_t get(size_t n){uint32_t v=0;for(size_t i=0;i<n;++i)v|=uint32_t(p[offset++])<<(i*8);return v;}
    void putText(const char *s,size_t n){memcpy(p+offset,s,n);offset+=n;}
    bool getText(char *s,size_t n){memcpy(s,p+offset,n);offset+=n;if(s[n-1])return false;for(size_t i=0;s[i];++i)if(uint8_t(s[i])<32||uint8_t(s[i])>126)return false;return true;}
};
void encodeDevice(const Device &d,uint8_t *raw) {
    Bytes b{raw};for(auto n:d.mac)b.put(n,1);
    b.put(d.ip,4);b.put(d.firstSeen,4);b.put(d.lastSeen,4);b.put(d.sightings,4);b.put(d.ports,2);b.put(d.evidence,1);b.put(d.known,1);
    b.putText(d.name,sizeof(d.name));b.putText(d.note,sizeof(d.note));b.putText(d.hostname,sizeof(d.hostname));b.putText(d.mdns,sizeof(d.mdns));b.putText(d.advertised,sizeof(d.advertised));
}
bool decodeDevice(uint8_t *raw,Device &d,const Network &network) {
    Bytes b{raw};d={};for(auto &n:d.mac)n=b.get(1);
    d.ip=b.get(4);d.firstSeen=b.get(4);d.lastSeen=b.get(4);d.sightings=b.get(4);d.ports=b.get(2);d.evidence=b.get(1);uint32_t known=b.get(1);d.known=known;
    return validMac(d.mac)&&network.contains(d.ip)&&known<=1&&d.sightings>0&&d.ports<(1u<<PORT_COUNT)&&d.evidence<32&&
        b.getText(d.name,sizeof(d.name))&&b.getText(d.note,sizeof(d.note))&&b.getText(d.hostname,sizeof(d.hostname))&&b.getText(d.mdns,sizeof(d.mdns))&&b.getText(d.advertised,sizeof(d.advertised))&&b.offset==RECORD_SIZE;
}
void encodeHeader(const Database &db,uint8_t *raw) {
    Bytes b{raw};b.putText("MWI1",4);b.put(db.count,2);
    b.put(db.network.local,4);b.put(db.network.mask,4);b.put(db.network.gateway,4);b.put(db.network.dns,4);
    for(auto n:db.network.bssid)b.put(n,1);
    b.putText(db.network.ssid,33);
    b.put(0,4); // reserved for format extension, always zero
}
bool readFile(FS &fs,const String &path,Database *out,const Network &expected) {
    File f=fs.open(path,FILE_READ);if(!f)return false;
    uint8_t raw[RECORD_SIZE]{};uint32_t crc=0xffffffff;
    auto read=[&](size_t n){if(f.read(raw,n)!=n)return false;for(size_t i=0;i<n;++i)crc=crcByte(crc,raw[i]);return true;};
    if(!read(HEADER_SIZE)||memcmp(raw,"MWI1",4))return false;
    Bytes b{raw};b.offset=4;size_t count=b.get(2);Network net;
    net.local=b.get(4);net.mask=b.get(4);net.gateway=b.get(4);net.dns=b.get(4);
    for(auto &n:net.bssid)n=b.get(1);
    // SSID is raw Wi-Fi metadata and may contain UTF-8. Never draw it without sanitizing.
    memcpy(net.ssid,raw+b.offset,33);b.offset+=33;
    if(net.ssid[32]||b.get(4)!=0||!net.valid()||!net.same(expected)||count>MAX_HISTORY||f.size()!=HEADER_SIZE+count*RECORD_SIZE+4)return false;
    for(size_t i=0;i<count;++i){
        Device d;if(!read(RECORD_SIZE)||!decodeDevice(raw,d,expected))return false;
        if(out){if(out->find(d.mac))return false;out->devices[out->count++]=d;}
    }
    if(f.read(raw,4)!=4)return false;
    Bytes end{raw};return end.get(4)==(crc^0xffffffff);
}
}
DeviceStore::DeviceStore(FS &storage,const Network &n):fs(storage) {
    char key[70];snprintf(key,sizeof(key),"/MaliInspector/%02X%02X%02X%02X%02X%02X_%08lX_%u.mwi",n.bssid[0],n.bssid[1],n.bssid[2],n.bssid[3],n.bssid[4],n.bssid[5],static_cast<unsigned long>(n.address()),prefixLength(n.mask));filename=key;
}
bool DeviceStore::recover(){
    String bak=filename+".bak";if(!fs.exists(bak))return true;
    return fs.exists(filename)?fs.remove(bak):fs.rename(bak,filename);
}
bool DeviceStore::load(Database &db){
    if(!recover())return false;
    if(!fs.exists(filename))return true;
    db.count=0;
    if(!readFile(fs,filename,&db,db.network)){db.count=0;for(auto &d:db.devices)d=Device{};return false;}
    return true;
}
bool DeviceStore::save(Database &db){
    if(!db.dirty)return true;
    if(!db.network.valid()||db.count>MAX_HISTORY||!recover())return false;
    if(!fs.exists("/MaliInspector")&&!fs.mkdir("/MaliInspector"))return false;
    if(!fs.exists(filename)) {
        size_t count=0;File dir=fs.open("/MaliInspector");if(!dir)return false;
        File entry=dir.openNextFile();while(entry){String n=entry.name();if(n.endsWith(".mwi")||n.endsWith(".mwi.bak"))++count;entry.close();entry=dir.openNextFile();}dir.close();
        if(count>=MAX_NETWORKS)return false;
    }
    String temp=filename+".tmp",bak=filename+".bak";
    if(fs.exists(temp)&&!fs.remove(temp))return false;
    File f=fs.open(temp,FILE_WRITE);if(!f)return false;
    uint8_t raw[RECORD_SIZE]{};uint32_t crc=0xffffffff;
    auto write=[&](size_t n){for(size_t i=0;i<n;++i)crc=crcByte(crc,raw[i]);return f.write(raw,n)==n;};
    encodeHeader(db,raw);bool ok=write(HEADER_SIZE);
    for(size_t i=0;ok&&i<db.count;++i){encodeDevice(db.devices[i],raw);ok=write(RECORD_SIZE);}
    Bytes end{raw};end.put(crc^0xffffffff,4);ok=ok&&f.write(raw,4)==4;f.flush();f.close();
    if(!ok||!readFile(fs,temp,nullptr,db.network)){fs.remove(temp);return false;}
    bool exists=fs.exists(filename);
    if(exists&&!fs.rename(filename,bak)){fs.remove(temp);return false;}
    if(!fs.rename(temp,filename)){if(exists)fs.rename(bak,filename);fs.remove(temp);return false;}
    if(exists)fs.remove(bak);
    db.dirty=false;return true;
}
bool DeviceStore::erase(){
    // Remove backup first, so deleting a database cannot resurrect it on the next open.
    for(const auto &suffix:{String(".bak"),String(".tmp"),String("")}){String p=filename+suffix;if(fs.exists(p)&&!fs.remove(p))return false;}
    return true;
}
}
