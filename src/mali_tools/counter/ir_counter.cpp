#include "core/ui/PtBr.h"
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
            parsed = MaliText::protocol_a57e91 + typeToString(result.decode_type) +
                     MaliText::address_c0ea3e + String(result.address, HEX) + MaliText::command_7c603c + String(result.command, HEX);
            data.lines[0] = MaliText::signals_4c3892 + String(signals) + MaliText::repeats_fd9d82 + String(repeats);
            data.lines[1] = MaliText::protocol_a57e91 + typeToString(result.decode_type) + " bits: " + String(result.bits);
            data.lines[2] = MaliText::address_0x_be81a9 + String(result.address, HEX);
            data.lines[3] = MaliText::command_0x_3fcec2 + String(result.command, HEX);
            data.lines[4] = MaliText::interval_e40a07 + String(interval) + " ms";
            data.lines[6] = MaliText::carrier_n_a_demodulated_rx_02855b;
            data.lines[7] = MaliText::ok_save_sample_raw_timings_cb5de1;
            data.status = repeat ? MaliText::repeating_signal_b6d450 : "NORMAL";
            if (!repeat) event(IR, typeToString(result.decode_type) + MaliText::signal_a7c495);
            receiver->resume();
        }
        if (now - window >= 1000) {
            for (int i = 0; i < 31; ++i) data.bars[i] = data.bars[i + 1];
            data.bars[31] = min(100, int(rate) * 5);
            if (rate > 20) {
                data.status = MaliText::high_ir_activity_eb5f4e;
                event(IR, data.status, true);
            } else if (now - last > 1000) data.status = "NORMAL";
            data.warning = rate > 20;
            rate = 0;
            window = now;
        }
        data.lines[5] = signals ? MaliText::last_4b243c + String((now - last) / 1000.0f, 1) + MaliText::s_ago_090b41 : MaliText::last_d01783;
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
        size_t written = f.println(MaliText::malios_received_ir_sample_carrier_unknown_55d19c);
        written += f.println(parsed);
        written += f.println(String(MaliText::truncated_00e0dc) + (overflow ? "yes" : "no"));
        written += f.println(MaliText::alternating_mark_space_durations_us_d4bcdf);
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
