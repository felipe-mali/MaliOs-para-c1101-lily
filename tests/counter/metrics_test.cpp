#define HIGH 1
#define LOW 0
#include "../../src/mali_tools/counter/counter_metrics.h"
#include "../../src/modules/rfid/pn532_uid.h"
#include <array>
using namespace CounterSuite;
static_assert(wifiLevel(0, 0, 1, false) == TrafficLevel::Deauth, "deauth wins even at low traffic");
static_assert(wifiLevel(900, 100, 1, true) == TrafficLevel::Deauth, "deauth precedence");
static_assert(wifiLevel(301, 100, 0, false) == TrafficLevel::Suspicious, "baseline spike");
static_assert(wifiLevel(300, 100, 0, false) == TrafficLevel::Busy, "strict spike boundary");
static_assert(wifiLevel(501, 0, 0, false) == TrafficLevel::HighTraffic, "cold start has no baseline alarm");
static_assert(wifiLevel(100, 0, 0, false) == TrafficLevel::Normal, "normal boundary");
static_assert(wifiLevel(0, 0, 0, true) == TrafficLevel::Suspicious, "AP churn independent of rate");
static_assert(perSecond(5000000, 1000) == 5000000, "rate multiplication must not overflow 32 bits");
static_assert(perSecond(5, 0) == 0, "zero-duration window");
static_assert(uint32_t(10U - 0xfffffff0U) == 26U, "millis rollover uses unsigned intervals");
static_assert(validRfFrequency(300) && validRfFrequency(928), "supported endpoints");
static_assert(!validRfFrequency(349) && !validRfFrequency(778) && !validRfFrequency(929), "band gaps");
constexpr std::array<uint8_t, 26> frame(uint8_t uidLength) {
    std::array<uint8_t, 26> f{};
    f[2] = 0xff;
    f[3] = 8 + uidLength;
    f[4] = uint8_t(0 - f[3]);
    f[5] = 0xd5;
    f[6] = 0x4b;
    f[7] = 1;
    f[12] = uidLength;
    return f;
}
constexpr auto shortUid = frame(4), normalUid = frame(7), longUid = frame(10), oversizedUid = frame(255),
               invalidUid = frame(8);
static_assert(pn532UidLength(shortUid.data(), 26) == 4, "four byte UID");
static_assert(pn532UidLength(normalUid.data(), 26) == 7, "seven byte UID");
static_assert(pn532UidLength(longUid.data(), 26) == 10, "ten byte UID must not be truncated");
static_assert(pn532UidLength(longUid.data(), 20) == 0, "truncated long UID rejected");
static_assert(pn532UidLength(oversizedUid.data(), 26) == 0, "hostile UID length rejected before copy");
static_assert(pn532UidLength(invalidUid.data(), 26) == 0, "unsupported length");
static_assert(
    pn532UidLength(nullptr, 26) == 0 && pn532UidLength(shortUid.data(), 12) == 0, "short header rejected"
);
constexpr auto badHeader = []() {
    auto f = frame(4);
    f[4] ^= 1;
    return f;
}();
static_assert(pn532UidLength(badHeader.data(), 26) == 0, "invalid length checksum rejected");
