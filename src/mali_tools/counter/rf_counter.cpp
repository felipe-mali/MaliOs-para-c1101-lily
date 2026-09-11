#include "core/led_control.h"
#include "core/mykeyboard.h"
#include "counter_main.h"
#include "counter_metrics.h"
#include "modules/rf/rf_utils.h"
#include <globals.h>
namespace CounterSuite {
namespace {
float spectrumCenter = 433.92f;
class RfMonitor : public Monitor {
    bool watch, owned = false, tuned = false, attempted = false;
    float center = 433.92f;
    uint8_t index = 0;
    uint32_t hits[41] = {}, logAt = 0, windowAt = 0, windowEvents = 0, unusualAt = 0, warningAt = 0;
    bool unusual = false;
    uint8_t busiest = 0;
    int peaks[41];
    bool active[41] = {};
    uint32_t at = 0, started[41] = {}, events = 0, duration = 0, samples = 0, last = 0;
    int64_t sum = 0;
    int peak = -127;
    float peakFreq = 0;

public:
    explicit RfMonitor(bool w) : watch(w), center(spectrumCenter) {
        for (auto &v : peaks) v = -127;
    }
    bool begin() override {
#if defined(USE_CC1101_VIA_SPI)
        if (bruceConfigPins.rfModule != CC1101_SPI_MODULE) return false;
        const auto &pins = bruceConfigPins.CC1101_bus;
        if (pins.sck == GPIO_NUM_NC || pins.miso == GPIO_NUM_NC || pins.mosi == GPIO_NUM_NC ||
            pins.cs == GPIO_NUM_NC || pins.io0 == GPIO_NUM_NC)
            return false;
        String input = watch ? keyboard("433.920", 12, "RF Watch MHz") : String(center, 3);
        if (input.isEmpty()) return false;
        char *end = nullptr;
        center = strtof(input.c_str(), &end);
        if (!end || *end || !isfinite(center) || !validRfFrequency(center)) return false;
        attempted = true;
        owned = initRfModule("rx", center);
        if (owned && !watch) ELECHOUSE_cc1101.setRxBW(58);
        data.barCount = watch ? 1 : 41;
        data.status = "NO SIGNAL";
        return owned;
#else
        return false;
#endif
    }
    void tick(uint32_t now) override {
#if defined(USE_CC1101_VIA_SPI)
        float freq = watch ? center : center + (int(index) - 20) * .01f;
        if (!tuned) {
            setMHZ(freq);
            at = now;
            tuned = true;
            return;
        }
        if (now - at < max(uint32_t(5),sampleInterval)) return;
        int rssi = ELECHOUSE_cc1101.getRssi();
        sum += rssi;
        ++samples;
        metrics.samples = samples; metrics.rssi = rssi;
        peaks[index] = max(peaks[index], rssi);
        if (rssi > peak) {
            peak = rssi;
            peakFreq = freq;
        }
        bool signal = rssi >= -75;
        if (signal) {
            ++hits[index];
            if (hits[index] > hits[busiest]) busiest = index;
        }
        if (signal && !active[index]) {
            started[index] = now;
            ++events;
            metrics.events = metrics.rx = events;
            ++windowEvents;
            last = now;
            if (now - logAt >= 500) {
                event(RF, String(freq, 3) + " MHz detected");
                if (rssi >= -45) setLedState(MaliLedState::COUNTER_STRONG);
                logAt = now;
            }
        }
        if (signal) duration = now - started[index];
        String status = signal ? duration > 3000 ? "CONTINUOUS SIGNAL"
                                 : rssi >= -45   ? "STRONG SIGNAL"
                                                 : "SIGNAL"
                               : "NO SIGNAL";
        if (now - windowAt >= 1000) {
            if (windowEvents > 30) {
                unusual = true;
                unusualAt = now;
            }
            windowEvents = 0;
            windowAt = now;
        }
        if (unusual && now - unusualAt >= 3000) unusual = false;
        if (unusual) status = "UNUSUAL ACTIVITY";
        if (status != data.status && now - warningAt >= 2000 &&
            (status == "CONTINUOUS SIGNAL" || status == "UNUSUAL ACTIVITY")) {
            event(RF, status, true);
            warningAt = now;
        }
        data.status = status;
        data.warning = status == "CONTINUOUS SIGNAL" || status == "UNUSUAL ACTIVITY";
        active[index] = signal;
        data.bars[index] = constrain((peaks[index] + 110) * 100 / 80, 0, 100);
        data.lines[0] = String(watch ? "WATCHING " : "Spectrum ") + String(center, 3) + " MHz";
        data.lines[1] = "Now: " + String(freq, 3) + " / " + String(rssi) + " dBm";
        data.lines[2] = "Peak hold: " + String(peak) + " dBm";
        data.lines[3] = "Peak freq: " + String(peakFreq, 3) + " MHz";
        data.lines[4] = "Avg RSSI: " + String(int(sum / samples)) + " dBm";
        data.lines[5] = "Events: " + String(events) + " duration ~" + String(duration) + " ms";
        data.lines[6] = last ? "Last: up " + String(last / 1000) + "s" : "Last: --";
        data.lines[7] = "Bars: peak hold -110..-30 dBm";
        data.lines[8] = watch ? "Continuo = RSSI >= -75 dBm"
                              : String(center - .2f, 3) + ".." + String(center + .2f, 3) + " / 10 kHz";
        data.lines[9] =
            "Most active: " + String(watch ? center : center + (int(busiest) - 20) * .01f, 3) + " MHz";
        data.lines[10] = "Duracao aproximada / amostrada";
        index = watch ? 0 : (index + 1) % 41;
        tuned = watch;
        at = now;
#endif
    }
    void save() override {
        for (auto &v : peaks) v = -127;
        peak = -127;
        peakFreq = 0;
        for (auto &v : data.bars) v = 0;
    }
    void end() override {
        if (attempted) {
            deinitRfModule();
            owned = false;
            attempted = false;
        }
    }
};
} // namespace
void setSpectrumCenter(float mhz) { spectrumCenter = mhz; }
std::unique_ptr<Monitor> makeRf(bool watch) { return std::unique_ptr<Monitor>(new RfMonitor(watch)); }
} // namespace CounterSuite
