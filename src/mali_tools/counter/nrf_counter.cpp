#include "counter_main.h"
#include "modules/NRF24/nrf_common.h"
#include <globals.h>
#if defined(USE_W5500_VIA_SPI) && !defined(LITE_VERSION)
#include <ETH.h>
#endif
namespace CounterSuite {
namespace {
bool pinsFree() {
#if defined(USE_NRF24_VIA_SPI)
    auto &p = bruceConfigPins.NRF24_bus;
    if (gpsConnected || p.sck == GPIO_NUM_NC || p.miso == GPIO_NUM_NC || p.mosi == GPIO_NUM_NC ||
        p.cs == GPIO_NUM_NC || p.io0 == GPIO_NUM_NC)
        return false;
#if defined(USE_W5500_VIA_SPI) && !defined(LITE_VERSION)
    if (ETH.started()) return false;
#endif
    return true;
#else
    return false;
#endif
}
void stop() {
#if defined(USE_NRF24_VIA_SPI)
    NRFradio.stopListening();
    NRFradio.powerDown();
    digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
    digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
#endif
}
class NrfMonitor : public Monitor {
    bool owned = false;
    uint8_t ch = 0, busiest = 0;
    uint32_t events = 0, hits[80] = {}, samples[80] = {};

public:
    bool begin() override {
#if defined(USE_NRF24_VIA_SPI)
        if (!pinsFree()) return false;
        owned = true;
        if (!nrf_start(NRF_MODE_SPI) || !NRFradio.isChipConnected()) return false;
        NRFradio.setAutoAck(false);
        NRFradio.disableCRC();
        data.barCount = 80;
        data.status = "NO SIGNAL";
        return true;
#else
        return false;
#endif
    }
    void tick(uint32_t now) override {
#if defined(USE_NRF24_VIA_SPI)
        NRFradio.setChannel(ch);
        NRFradio.startListening();
        delayMicroseconds(160);
        bool hit = NRFradio.testRPD();
        NRFradio.stopListening();
        ++samples[ch];
        if (hit) {
            ++hits[ch];
            ++events;
        }
        data.bars[ch] = 100 * hits[ch] / samples[ch];
        if (data.bars[ch] > data.bars[busiest]) busiest = ch;
        data.status = hit ? "SIGNAL" : "NO SIGNAL";
        data.lines[0] = "RPD hits: " + String(events);
        data.lines[1] = "Most active: " + String(2400 + busiest) + " MHz";
        data.lines[2] = "Channel: " + String(ch) + " occupancy: " + String(data.bars[ch]) + "%";
        data.lines[3] = "2400..2479 MHz / RPD threshold";
        data.lines[4] = "Energy samples, not devices";
        if (ch == 79 && events) event(NRF, "RPD sweep: " + String(events) + " cumulative hits");
        ch = (ch + 1) % 80;
#endif
    }
    void end() override {
        if (owned) {
            stop();
            owned = false;
        }
    }
};
} // namespace
bool nrfAvailable() {
    if (!pinsFree()) return false;
#if defined(USE_NRF24_VIA_SPI)
    bool found = nrf_start(NRF_MODE_SPI) && NRFradio.isChipConnected();
    stop();
    return found;
#else
    return false;
#endif
}
std::unique_ptr<Monitor> makeNrf() { return std::unique_ptr<Monitor>(new NrfMonitor); }
} // namespace CounterSuite
