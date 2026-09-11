#include "core/display.h"
#include "core/sd_functions.h"
#include "counter_main.h"
#include <IRrecv.h>
#include <IRutils.h>
#include <globals.h>
namespace CounterSuite {
namespace {
class IrMonitor : public Monitor {
    std::unique_ptr<IRrecv> receiver;
    decode_results result{};
    uint32_t signals = 0, repeats = 0, last = 0, interval = 0, window = 0, rate = 0;
    uint32_t polledAt = 0;
    uint64_t value = 0;
    decode_type_t protocol = UNKNOWN;
    uint16_t raw[1024] = {};
    uint16_t rawLength = 0;
    bool overflow = false;
    String parsed;

public:
    bool begin() override {
        if (bruceConfigPins.irRx == GPIO_NUM_NC) return false;
        receiver.reset(new IRrecv(bruceConfigPins.irRx, 1024, 50, true));
        receiver->enableIRIn();
        data.status = "NORMAL";
        data.barCount = 32;
        window = millis();
        return true;
    }
    void tick(uint32_t now) override {
        if (sampleInterval && now - polledAt < sampleInterval) return;
        polledAt = now;
        if (receiver->decode(&result)) {
            interval = signals ? now - last : 0;
            bool repeat = result.repeat || (signals && result.value == value &&
                                            result.decode_type == protocol && interval < 300);
            if (repeat) ++repeats;
            ++signals;
            metrics.events = metrics.rx = signals; metrics.retries = repeats;
            if (result.overflow) ++metrics.failures;
            metrics.success = signals - metrics.failures;
            if (signals > 1) {
                ++metrics.samples;
                metrics.timeTotal += interval;
                metrics.timeMin = min(metrics.timeMin, interval); metrics.timeMax = max(metrics.timeMax, interval);
            }
            ++rate;
            last = now;
            value = result.value;
            protocol = result.decode_type;
            rawLength = constrain(int(result.rawlen) - 1, 0, 1024);
            overflow = result.overflow;
            for (int i = 0; i < rawLength; ++i) raw[i] = result.rawbuf[i + 1];
            parsed = "Protocol: " + typeToString(result.decode_type) +
                     " address: " + String(result.address, HEX) + " command: " + String(result.command, HEX);
            data.lines[0] = "Signals: " + String(signals) + " repeats: " + String(repeats);
            data.lines[1] = "Protocol: " + typeToString(result.decode_type) + " bits: " + String(result.bits);
            data.lines[2] = "Address: 0x" + String(result.address, HEX);
            data.lines[3] = "Command: 0x" + String(result.command, HEX);
            data.lines[4] = "Interval: " + String(interval) + " ms";
            data.lines[6] = "Carrier: N/A (demodulated RX)";
            data.lines[7] = "OK: Save Sample (raw timings)";
            data.status = repeat ? "REPEATING SIGNAL" : "NORMAL";
            if (!repeat) event(IR, typeToString(result.decode_type) + " signal");
            receiver->resume();
        }
        if (now - window >= 1000) {
            for (int i = 0; i < 31; ++i) data.bars[i] = data.bars[i + 1];
            data.bars[31] = min(100, int(rate) * 5);
            if (rate > 20) {
                data.status = "HIGH IR ACTIVITY";
                event(IR, data.status, true);
            } else if (now - last > 1000) data.status = "NORMAL";
            data.warning = rate > 20;
            rate = 0;
            window = now;
        }
        data.lines[5] = signals ? "Last: " + String((now - last) / 1000.0f, 1) + " s ago" : "Last: --";
    }
    void save() override {
        if (!rawLength) {
            displayError("Sem captura", true);
            return;
        }
        if (!setupSdCard()) {
            displayError("SD indisponivel", true);
            return;
        }
        SD.mkdir("/MaliCounter");
        String path = "/MaliCounter/ir_" + String(millis()) + ".txt";
        File f = SD.open(path, FILE_WRITE);
        if (!f) {
            displayError("Falha ao salvar", true);
            return;
        }
        size_t written = f.println("MaliOS received IR sample; carrier unknown");
        written += f.println(parsed);
        written += f.println(String("Truncated: ") + (overflow ? "yes" : "no"));
        written += f.println("Alternating mark/space durations (us):");
        for (int i = 0; i < rawLength; ++i) {
            written += f.print(uint32_t(raw[i]) * kRawTick);
            written += f.print(' ');
        }
        bool ok = written > 0 && !f.getWriteError();
        f.close();
        if (ok) displayInfo(path, true);
        else displayError("Falha ao salvar", true);
    }
    void end() override {
        if (receiver) {
            receiver->disableIRIn();
            receiver.reset();
        }
    }
};
} // namespace
std::unique_ptr<Monitor> makeIr() { return std::unique_ptr<Monitor>(new IrMonitor); }
} // namespace CounterSuite
