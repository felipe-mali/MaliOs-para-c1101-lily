#pragma once
#include "KeyProfile.h"
#include <array>

namespace MaliKeys {
// Portable, fixed size MLK1 format; explicit little endian values, no struct padding.
constexpr size_t RECORD_SIZE = 662;
using Record = std::array<uint8_t, RECORD_SIZE>;
constexpr uint32_t checksum(const Record &bytes) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < RECORD_SIZE - 4; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
struct Writer {
    Record &data; size_t pos = 0;
    constexpr void byte(uint8_t n) { data[pos++] = n; }
    constexpr void word(uint16_t n) { byte(n & 255); byte(n >> 8); }
    template<size_t N> constexpr void text(const char (&s)[N]) {
        bool end = false;
        for (size_t i = 0; i < N; ++i) { end = end || !s[i]; byte(end ? 0 : uint8_t(s[i])); }
    }
};
struct Reader {
    const Record &data; size_t pos = 0;
    constexpr uint8_t byte() { return data[pos++]; }
    constexpr uint16_t word() { uint16_t n = byte(); return n | (uint16_t(byte()) << 8); }
    template<size_t N> constexpr void text(char (&s)[N]) { for (size_t i=0; i<N; ++i) s[i] = char(byte()); }
};
constexpr bool encode(const KeyProfile &p, Record &bytes) {
    if (!validProfile(p)) return false;
    Writer w{bytes};
    w.byte('M'); w.byte('L'); w.byte('K'); w.byte('1'); w.byte(1); w.byte(uint8_t(p.type));
    w.text(p.name); w.text(p.manufacturer); w.text(p.profileType); w.text(p.notes); w.text(p.created);
    w.word(p.length); w.word(p.useful); w.word(p.width); w.word(p.thickness);
    w.byte(uint8_t(p.orientation)); w.byte(uint8_t(p.head));
    w.byte(p.flat.positions); w.byte(uint8_t(p.flat.side)); w.byte(p.flat.grooves);
    for (const auto &f : p.cross.faces) {
        w.byte(f.positions); w.byte(f.visualSpacing); w.word(f.length); w.word(f.arm); w.word(f.width);
        w.byte(uint8_t(f.orientation)); w.text(f.notes);
    }
    if (w.pos != RECORD_SIZE - 4) return false;
    uint32_t crc = checksum(bytes);
    for (int i=0; i<4; ++i) w.byte(uint8_t(crc >> (8*i)));
    return true;
}
constexpr bool decode(const Record &bytes, KeyProfile &out) {
    if (bytes[0]!='M' || bytes[1]!='L' || bytes[2]!='K' || bytes[3]!='1' || bytes[4]!=1) return false;
    uint32_t crc = 0;
    for (int i=0; i<4; ++i) crc |= uint32_t(bytes[RECORD_SIZE-4+i]) << (8*i);
    if (crc != checksum(bytes)) return false;
    Reader r{bytes, 5}; KeyProfile p;
    p.type = KeyType(r.byte());
    r.text(p.name); r.text(p.manufacturer); r.text(p.profileType); r.text(p.notes); r.text(p.created);
    p.length=r.word(); p.useful=r.word(); p.width=r.word(); p.thickness=r.word();
    p.orientation=Orientation(r.byte()); p.head=HeadShape(r.byte());
    p.flat.positions=r.byte(); p.flat.side=ProfileSide(r.byte()); p.flat.grooves=r.byte();
    for (auto &f : p.cross.faces) {
        f.positions=r.byte(); f.visualSpacing=r.byte(); f.length=r.word(); f.arm=r.word(); f.width=r.word();
        f.orientation=Orientation(r.byte()); r.text(f.notes);
    }
    if (!validProfile(p)) return false;
    out = p; return true;
}
} // namespace MaliKeys
