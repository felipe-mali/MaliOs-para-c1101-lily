#pragma once
#include "device_model.h"
#include <cstdio>
namespace WifiInspector {
struct NameInfo { char hostname[64]{},mdns[64]{},service[80]{}; };
inline uint16_t read16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t read32(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
// Decode DNS compression with bounded jumps and backward pointers only.
inline bool dnsName(const uint8_t *packet,size_t length,size_t &offset,char *out,size_t capacity){
    size_t pos=offset,written=0;bool jumped=false;unsigned hops=0;
    while(pos<length && hops++<128){
        uint8_t n=packet[pos++];
        if(!n){if(!jumped)offset=pos;if(written>=capacity)return false;out[written]=0;return true;}
        if((n&0xc0)==0xc0){if(pos>=length)return false;size_t target=((n&0x3f)<<8)|packet[pos++];if(target>=pos-2)return false;if(!jumped)offset=pos;jumped=true;pos=target;continue;}
        if(n>63||pos+n>length||written+n+(written?1:0)>=capacity)return false;
        if(written)out[written++]='.';
        for(unsigned i=0;i<n;++i){uint8_t c=packet[pos++];if(c<32||c>126)return false;out[written++]=char(c);}
        if(!jumped)offset=pos;
    }
    return false;
}
inline size_t dnsQuestion(uint8_t *out,size_t cap,const char *name,uint16_t id,bool mdns){
    if(cap<18)return 0;
    memset(out,0,cap);out[0]=id>>8;out[1]=id;out[2]=mdns?0:1;out[5]=1;
    size_t at=12;const char *part=name;
    while(*part){const char *end=strchr(part,'.');size_t n=end?size_t(end-part):strlen(part);if(!n||n>63||at+n+6>=cap)return 0;out[at++]=n;memcpy(out+at,part,n);at+=n;if(!end)break;part=end+1;}
    out[at++]=0;out[at++]=0;out[at++]=12; // PTR; mDNS uses QU to request unicast replies on our own socket.
    out[at++]=mdns?0x80:0;out[at++]=1;return at;
}
inline bool parseDns(const uint8_t *p,size_t size,uint32_t ip,const char *reverse,uint16_t id,bool mdns,NameInfo &result){
    if(size<12||size>1500||!(p[2]&0x80)||(p[2]&0x7a)||(p[3]&15)||read16(p)!=id)return false;
    unsigned questions=read16(p+4),records=unsigned(read16(p+6))+read16(p+8)+read16(p+10);
    if(questions>16||records>64)return false;
    size_t at=12;char owner[256],value[256];NameInfo next{};
    for(unsigned i=0;i<questions;++i){if(!dnsName(p,size,at,owner,sizeof(owner))||at+4>size)return false;at+=4;}
    size_t start=at;
    // A records associate the hostname with the exact IPv4 being inspected.
    for(int pass=0;pass<2;++pass){
        at=start;
        for(unsigned i=0;i<records;++i){
            if(!dnsName(p,size,at,owner,sizeof(owner))||at+10>size)return false;
            uint16_t type=read16(p+at),cls=read16(p+at+2)&0x7fff,len=read16(p+at+8);uint32_t ttl=read32(p+at+4);at+=10;
            if(at+len>size)return false;
            size_t end=at+len,pos=at;
            if(cls==1&&ttl){
                if(pass==0&&type==1&&len==4&&read32(p+at)==ip&&mdns)cleanText(next.mdns,sizeof(next.mdns),owner);
                if(type==12){
                    if(!dnsName(p,size,pos,value,sizeof(value))||pos!=end)return false;
                    if(!strcmp(owner,reverse))cleanText(mdns?next.mdns:next.hostname,64,value);
                }
                if(pass==1&&mdns&&type==33){
                    if(len<7)return false;
                    pos+=6;if(!dnsName(p,size,pos,value,sizeof(value))||pos!=end)return false;
                    if(next.mdns[0]&&!strcmp(value,next.mdns))cleanText(next.service,sizeof(next.service),owner);
                }
            }
            at=end;
        }
    }
    result=next;return true;
}
inline void reverseName(uint32_t ip,char *out,size_t size){snprintf(out,size,"%u.%u.%u.%u.in-addr.arpa",unsigned(ip&255),unsigned((ip>>8)&255),unsigned((ip>>16)&255),unsigned(ip>>24));}
inline bool parseSsdp(const char *text,size_t n,char *out,size_t cap){
    if(n<12||n>1499||memcmp(text,"HTTP/1.1 200",12))return false;
    // Keep a single advertised service; never follow LOCATION URLs from an untrusted device.
    for(size_t at=0;at+4<n;++at){
        if((at==0||text[at-1]=='\n')&&(text[at]=='S'||text[at]=='s')&&(text[at+1]=='T'||text[at+1]=='t')&&text[at+2]==':'){
            size_t i=at+3,w=0;while(i<n&&text[i]==' ')++i;
            while(i<n&&text[i]!='\r'&&text[i]!='\n'&&w+1<cap){unsigned char c=text[i++];out[w++]=(c>=32&&c<=126)?char(c):'?';}out[w]=0;return w>0;
        }
    }
    return false;
}
}
