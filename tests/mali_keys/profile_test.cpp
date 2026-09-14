#include "../../src/mali_tools/keys/KeyCodec.h"
#include "../../src/mali_tools/keys/KeyComparison.h"
#include "../../src/core/ui/MaliMotion.h"
using namespace MaliKeys;
constexpr KeyProfile fixture(KeyType type) {
    KeyProfile p;
    p.name[0]='T'; p.name[1]='1'; p.manufacturer[0]='X'; p.profileType[0]='R'; p.notes[0]='N';
    p.type=type;p.length=6000;p.useful=4000;p.width=800;p.thickness=200;
    p.flat.positions=6;p.flat.grooves=2;p.flat.side=ProfileSide::Both;p.head=HeadShape::Oval;
    for (int i=0;i<4;++i) {
        p.cross.faces[i].positions=uint8_t(4+i);p.cross.faces[i].visualSpacing=uint8_t(50+i*30);
        p.cross.faces[i].arm=500+i*10;p.cross.faces[i].width=200+i*10;
        p.cross.faces[i].length=3900-i*100;p.cross.faces[i].notes[0]=char('A'+i);
        p.cross.faces[i].orientation=i%2?Orientation::Left:Orientation::Right;
    }
    return p;
}
constexpr bool roundtrip(KeyType type) {
    auto original=fixture(type); Record bytes{}; KeyProfile loaded;
    if(!encode(original,bytes) || !decode(bytes,loaded))return false;
    Record again{};if(!encode(loaded,again))return false;
    for(size_t i=0;i<bytes.size();++i)if(bytes[i]!=again[i])return false;
    return loaded.type==type && loaded.length==6000 && loaded.flat.grooves==2 &&
        loaded.cross.faces[3].notes[0]=='D' && loaded.cross.faces[3].orientation==Orientation::Left;
}
constexpr bool corrupt() {
    Record bytes{};auto p=fixture(KeyType::Flat);if(!encode(p,bytes))return false;
    // Corrupt distinct content areas, not just the magic header.
    for(size_t offset : {size_t(20),size_t(300),size_t(500),size_t(661)}) {
        Record changed=bytes;changed[offset]^=1;
        KeyProfile out;out.length=123;
        if(decode(changed,out) || out.length!=123)return false;
    }
    // Correct CRC cannot make an unsupported schema or enum valid.
    bytes[5]=99;uint32_t crc=checksum(bytes);
    for(int i=0;i<4;++i)bytes[RECORD_SIZE-4+i]=uint8_t(crc>>(8*i));
    return !decode(bytes,p);
}
constexpr bool invalid() {
    auto p=fixture(KeyType::Flat);
    p.useful=6001;if(validProfile(p))return false;
    p.useful=4000;p.cross.faces[2].length=4001;if(validProfile(p))return false;
    p.cross.faces[2].length=4000;p.cross.faces[1].visualSpacing=49;if(validProfile(p))return false;
    p.cross.faces[1].visualSpacing=100;p.flat.positions=21;if(validProfile(p))return false;
    p.flat.positions=0;p.length=0;p.useful=0;return validProfile(p);
}
constexpr bool textBounds() {
    KeyProfile p;for(char &c:p.name)c='X';
    if(validProfile(p))return false;
    p.name[31]=0;if(!validProfile(p))return false;
    p.notes[0]='\n';return !validProfile(p);
}
static_assert(roundtrip(KeyType::Flat),"Flat data must survive exact serialization");
static_assert(roundtrip(KeyType::Cruciform),"All four faces must survive serialization");
static_assert(corrupt(),"Reject corrupt data without mutating caller state");
static_assert(invalid(),"Validate dimensions, counts and display spacing");
static_assert(textBounds(),"Enforce bounded terminated strings");
static_assert(!validName("../bad") && !validName("/bad") && !validName(" bad") && !validName("bad "));
static_assert(validName("Chave A-1") && !validName("A:1"));
static_assert(validDate("2024-02-29") && !validDate("2025-02-29") && !validDate("2026-04-31"));
static_assert(adjusted(100,1000000000,100,0,30000)==30000);
static_assert(adjusted(100,-1000000000,100,0,30000)==0);
static_assert(positionCount(fixture(KeyType::Cruciform))==22);
static_assert(positionCount(KeyProfile{})==0);
constexpr bool buttons() {
    MaliUI::HoldButton b;
    if(b.update(true,0)!=MaliUI::ButtonEvent::None)return false;
    b.update(false,10);b.update(true,20);
    if(b.update(true,670)!=MaliUI::ButtonEvent::Back)return false;
    if(b.update(false,700)!=MaliUI::ButtonEvent::None)return false;
    b.update(true,710);return b.update(false,750)==MaliUI::ButtonEvent::Select;
}
static_assert(buttons(),"Hold cancels and cannot leak a click into the parent screen");
