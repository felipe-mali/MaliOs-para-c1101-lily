#include "device_classifier.h"
#include "oui_database.h"
#include <cctype>
namespace WifiInspector {
namespace {
bool contains(const char *s,const char *part) {
    for(;*s;++s){size_t i=0;while(part[i] && s[i] && std::tolower(static_cast<unsigned char>(s[i]))==std::tolower(static_cast<unsigned char>(part[i])))++i;if(!part[i])return true;}
    return false;
}
bool hint(const Device &d,const char *s){return contains(d.hostname,s)||contains(d.mdns,s)||contains(d.advertised,s);}
}
const char *classify(const Device &d,uint32_t gateway) {
    const char *v=manufacturer(d.mac);
    if(d.ip==gateway)return "Gateway da conexao";
    if(hasPort(d,515)||hasPort(d,631)||hasPort(d,9100)||hint(d,"_ipp")||hint(d,"printer"))return "Provavel impressora";
    if(hasPort(d,554))return "Possivel camera/DVR";
    if(hasPort(d,8008)||hint(d,"googlecast"))return "Possivel Google/Cast";
    if(hasPort(d,445))return "Possivel PC/NAS/servidor";
    if(!strcmp(v,"Raspberry Pi"))return "Possivel Raspberry Pi/SBC";
    if(hasPort(d,22))return "Possivel Linux/servidor";
    if(!strcmp(v,"HP")||!strcmp(v,"Brother")||!strcmp(v,"Epson")||!strcmp(v,"Zebra"))return "Possivel impressora/PC";
    if(!strcmp(v,"TP-Link")||!strcmp(v,"Ubiquiti")||!strcmp(v,"Intelbras")||!strcmp(v,"D-Link")||!strcmp(v,"Mercusys"))return "Possivel equipamento de rede";
    if(!strcmp(v,"Apple")||!strcmp(v,"Samsung")||!strcmp(v,"Motorola")||!strcmp(v,"Xiaomi")||hint(d,"iphone")||hint(d,"android"))return "Possivel celular/tablet";
    if(hasPort(d,1883))return "Possivel servidor MQTT/IoT";
    if(!strcmp(v,"Espressif"))return "Possivel ESP/IoT";
    return "Nao identificado";
}
}
