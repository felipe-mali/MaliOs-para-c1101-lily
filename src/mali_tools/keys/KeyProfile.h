#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MaliKeys {
// Hundredths of a millimetre. Zero means not measured, never an inferred dimension.
using Measure = uint16_t;
enum class KeyType : uint8_t { Flat, Cruciform };
enum class HeadShape : uint8_t { Round, Oval, Rectangular, Angular };
enum class ProfileSide : uint8_t { Upper, Lower, Both };
enum class Orientation : uint8_t { Right, Left };
struct FlatKeyProfile {
    uint8_t positions = 0;
    ProfileSide side = ProfileSide::Upper;
    uint8_t grooves = 0;
};
struct KeyFace {
    uint8_t positions = 0;
    uint8_t visualSpacing = 100; // Relative display spacing; not a manufacturing pitch.
    Measure length = 0;
    Measure arm = 0; // From the centre of the cross to the outside of this arm.
    Measure width = 0;
    Orientation orientation = Orientation::Right;
    char notes[80] = {};
};
struct CruciformKeyProfile { KeyFace faces[4]; };
struct KeyProfile {
    char name[32] = {};
    char manufacturer[48] = {};
    char profileType[32] = {};
    char notes[160] = {};
    char created[11] = {}; // YYYY-MM-DD, empty when date is unknown.
    KeyType type = KeyType::Flat;
    Measure length = 0, useful = 0, width = 0, thickness = 0;
    Orientation orientation = Orientation::Right;
    HeadShape head = HeadShape::Round;
    FlatKeyProfile flat;
    CruciformKeyProfile cross;
};
template <size_t N> constexpr bool validText(const char (&text)[N]) {
    for (size_t i = 0; i < N; ++i) {
        if (!text[i]) return true;
        if (static_cast<uint8_t>(text[i]) < 32 || text[i] == 127) return false;
    }
    return false;
}
constexpr bool validName(const char *name) {
    if (!name || !name[0]) return false;
    for (size_t i = 0; i < 32; ++i) {
        char c = name[i];
        if (!c) return true;
        if (i == 31) return false;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ')) return false;
        if (c == ' ' && (i == 0 || name[i + 1] == 0)) return false;
    }
    return false;
}
constexpr bool validDate(const char (&s)[11]) {
    if (!s[0]) return true;
    if (s[4] != '-' || s[7] != '-' || s[10]) return false;
    for (int i = 0; i < 10; ++i)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    int y = (s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0';
    int m = (s[5]-'0')*10+s[6]-'0', d = (s[8]-'0')*10+s[9]-'0';
    if (y < 1900 || m < 1 || m > 12 || d < 1) return false;
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int last = days[m-1] + (m == 2 && y%4 == 0 && (y%100 != 0 || y%400 == 0));
    return d <= last;
}
constexpr bool validProfile(const KeyProfile &p, bool requireName = true) {
    if ((requireName && !validName(p.name)) || !validText(p.name) || !validText(p.manufacturer) ||
        !validText(p.profileType) || !validText(p.notes) || !validDate(p.created)) return false;
    if (uint8_t(p.type) > 1 || uint8_t(p.orientation) > 1 || uint8_t(p.head) > 3 ||
        uint8_t(p.flat.side) > 2 || p.flat.positions > 20 || p.flat.grooves > 8) return false;
    if (p.length > 30000 || p.useful > 30000 || p.width > 10000 || p.thickness > 5000 ||
        (p.length && p.useful > p.length)) return false;
    for (const auto &f : p.cross.faces)
        if (f.positions > 20 || f.visualSpacing < 50 || f.visualSpacing > 150 ||
            f.length > 30000 || f.arm > 5000 || f.width > 5000 || uint8_t(f.orientation) > 1 ||
            (p.useful && f.length > p.useful) || !validText(f.notes)) return false;
    return true;
}
constexpr int adjusted(int value, int64_t steps, int increment, int lo, int hi) {
    int64_t next = value + steps * increment;
    return next < lo ? lo : next > hi ? hi : int(next);
}
} // namespace MaliKeys
