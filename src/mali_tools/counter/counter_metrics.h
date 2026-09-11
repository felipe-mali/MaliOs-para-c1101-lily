#pragma once
#include <stdint.h>
namespace CounterSuite {
enum class TrafficLevel { Normal, Busy, HighTraffic, Suspicious, Deauth };
constexpr TrafficLevel wifiLevel(uint32_t rate, float baseline, uint32_t deauth, bool churn) {
    return deauth                                            ? TrafficLevel::Deauth
           : churn || (baseline > 20 && rate > baseline * 3) ? TrafficLevel::Suspicious
           : rate > 500                                      ? TrafficLevel::HighTraffic
           : rate > 100                                      ? TrafficLevel::Busy
                                                             : TrafficLevel::Normal;
}
constexpr bool validRfFrequency(float mhz) {
    return (mhz >= 300 && mhz <= 348) || (mhz >= 387 && mhz <= 464) || (mhz >= 779 && mhz <= 928);
}
constexpr uint32_t perSecond(uint32_t count, uint32_t elapsed) {
    return elapsed ? uint64_t(count) * 1000 / elapsed : 0;
}
} // namespace CounterSuite
