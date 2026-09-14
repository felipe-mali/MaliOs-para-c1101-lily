#include "core/ui/PtBr.h"
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
        data.status = MaliText::no_signal_b1069c;
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
        data.status = hit ? MaliText::signal_289e7c : MaliText::no_signal_b1069c;
        data.lines[0] = MaliText::rpd_hits_deb7c0 + String(events);
        data.lines[1] = MaliText::most_active_6f1e13 + String(2400 + busiest) + " MHz";
        data.lines[2] = MaliText::channel_a58aee + String(ch) + MaliText::occupancy_9ccb80 + String(data.bars[ch]) + "%";
        data.lines[3] = MaliText::text_2400_2479_mhz_rpd_threshold_51dae9;
        data.lines[4] = MaliText::energy_samples_not_devices_268b22;
        if (ch == 79 && events) event(NRF, MaliText::rpd_sweep_e2cb82 + String(events) + MaliText::cumulative_hits_867c94);
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
