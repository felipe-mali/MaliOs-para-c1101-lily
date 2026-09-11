#include "counter_main.h"
#include "core/display.h"
#include "core/led_control.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/utils.h"
#include "counter_events.h"
#include "counter_metrics.h"
#include <globals.h>
namespace CounterSuite {
namespace {
Snapshot latest[COUNT];
std::unique_ptr<Monitor> create(Module m, bool watch) {
    switch (m) {
        case WIFI: return makeWifi();
        case BLE: return makeBle();
        case RF: return makeRf(watch);
        case NFC: return makeNfc();
        case IR: return makeIr();
        case NRF: return makeNrf();
        default: return nullptr;
    }
}
void line(int y, const String &s, uint16_t color) {
    tft.setTextColor(color, bruceConfig.bgColor);
    tft.drawString(s.substring(0, (tftWidth - 16) / 6), 8, y, 1);
}
int textRows(const Snapshot &s) {
    int rows = 0, columns = max(1, (tftWidth - 16) / 6);
    for (const auto &text : s.lines)
        if (text.length()) rows += (text.length() + columns - 1) / columns;
    return max(1, rows);
}
void draw(Module m, Snapshot &s, uint8_t page, bool global) {
    drawMainBorderWithTitle(global ? String("GLOBAL / ") + name(m) : String(name(m)));
    tft.setTextSize(1);
    line(28, s.status, s.warning ? TFT_RED : TFT_GREEN);
    int available = max(1, (tftHeight - 88) / 13);
    int offset = page * available;
    int logical = 0, visible = 0, columns = max(1, (tftWidth - 16) / 6);
    for (const auto &text : s.lines) {
        for (unsigned pos = 0; pos < text.length(); pos += columns) {
            if (logical++ < offset) continue;
            if (visible >= available) break;
            line(44 + visible++ * 13, text.substring(pos, pos + columns), bruceConfig.priColor);
        }
    }
    if (s.barCount) {
        int width = max(1, (tftWidth - 16) / s.barCount);
        for (int i = 0; i < s.barCount; ++i) {
            int h = s.bars[i] * 20 / 100;
            tft.fillRect(8 + i * width, tftHeight - 24 - h, max(1, width - 1), h, bruceConfig.secColor);
        }
    }
    line(
        tftHeight - 15,
        m == IR   ? "ENC:pag OK:salvar BACK:sair"
        : m == RF ? "ENC:pag OK:peak reset BACK"
                  : "ENC:pagina BACK:sair",
        bruceConfig.secColor
    );
}
void dashboard() {
    std::vector<Option> rows;
    for (int i = 0; i < COUNT; ++i) {
        if (i == NRF && !nrfAvailable()) continue;
        Module m = static_cast<Module>(i);
        String title = String(name(m)) + " " + latest[i].status;
        if (latest[i].updated) title += " (" + String((millis() - latest[i].updated) / 1000) + "s)";
        rows.push_back({title.c_str(), [m]() { run(m); }});
    }
    rows.push_back({("Events: " + String(eventCount())).c_str(), logMenu});
    rows.push_back({"Voltar", []() {}});
    loopOptions(rows, MENU_TYPE_SUBMENU, "COUNTER SUITE / ultimo estado");
}
} // namespace
const char *name(Module m) {
    static const char *names[] = {
        "Wi-Fi Counter", "BLE Counter", "RF Counter", "NFC Counter", "IR Counter", "2.4G Counter"
    };
    return names[m];
}
void run(Module module, bool global, bool watch) {
    MaliLedStateGuard guard(MaliLedState::MENU);
    setupSdCard();
    uint8_t page = 0;
    uint32_t logAt = 0;
    uint32_t render = 0, started = millis(), ledAt = millis(), count = eventCount();
    auto monitor = create(module, watch);
    if (!monitor) return;
    bool ready = monitor->begin();
    if (!ready && monitor->data.status == "IDLE") monitor->data.status = "UNAVAILABLE / BUSY";
    check(SelPress);
    returnToMenu = false;
    while (!returnToMenu && !check(EscPress)) {
        uint32_t now = millis();
        if (ready) monitor->tick(now);
        if (check(NextPress) || check(PrevPress)) {
            int rows = max(1, (tftHeight - 88) / 13);
            page = (page + 1) % ((textRows(monitor->data) + rows - 1) / rows);
        }
        if (check(SelPress) && ready && (module == IR || module == RF)) monitor->save();
        if (now - logAt >= 1000) {
            flushLog();
            logAt = now;
        }
        if (count != eventCount()) {
            count = eventCount();
            ledAt = now;
        }
        if (now - ledAt > 250) setLedState(MaliLedState::MENU);
        if (now - render >= 200) {
            render = now;
            monitor->data.updated = now;
            latest[module] = monitor->data;
            draw(module, monitor->data, page, global);
        }
        if (global && now - started >= (ready ? 8000U : 500U)) {
            monitor->end();
            flushLog();
            module = static_cast<Module>((module + 1) % COUNT);
            if (module == NRF && !nrfAvailable()) module = WIFI;
            monitor = create(module, false);
            ready = monitor && monitor->begin();
            if (!ready && monitor->data.status == "IDLE") monitor->data.status = "UNAVAILABLE / BUSY";
            started = millis();
            page = 0;
        }
        delay(1);
    }
    monitor->end();
    flushLog();
}
void rfMenu() {
    std::vector<Option> rows = {
        {"Spectrum Monitor",   []() { run(RF); }             },
        {"Banda 315 MHz",
         []() {
             setSpectrumCenter(315.0f);
             run(RF);
         }                                                   },
        {"Banda 433 MHz",
         []() {
             setSpectrumCenter(433.92f);
             run(RF);
         }                                                   },
        {"Banda 868 MHz",
         []() {
             setSpectrumCenter(868.35f);
             run(RF);
         }                                                   },
        {"Banda 915 MHz",
         []() {
             setSpectrumCenter(915.0f);
             run(RF);
         }                                                   },
        {"Centro do espectro",
         []() {
             String input = num_keyboard("433.920", 12, "Centro MHz (+/-0.2)");
             char *end = nullptr;
             float mhz = strtof(input.c_str(), &end);
             if (end == input.c_str() || *end || !validRfFrequency(mhz - .2f) ||
                 !validRfFrequency(mhz + .2f)) {
                 displayError("Frequencia invalida", true);
                 return;
             }
             setSpectrumCenter(mhz);
             run(RF);
         }                                                   },
        {"RF Watch",           []() { run(RF, false, true); }},
        {"Voltar",             []() {}                       }
    };
    loopOptions(rows, MENU_TYPE_SUBMENU, "RF Counter");
}
void open() {
    MaliLedStateGuard idle(MaliLedState::IDLE);
    std::vector<Option> rows = {
        {"Dashboard",      dashboard                },
        {"Global Monitor", []() { run(WIFI, true); }},
        {"Wi-Fi Counter",  []() { run(WIFI); }      },
        {"BLE Counter",    []() { run(BLE); }       },
        {"RF Counter",     rfMenu                   },
        {"NFC Counter",    []() { run(NFC); }       },
        {"IR Counter",     []() { run(IR); }        },
        {"Event Log",      logMenu                  }
    };
    if (nrfAvailable()) rows.push_back({"2.4G Counter", []() { run(NRF); }});
    rows.push_back({"Voltar", []() {}});
    loopOptions(rows, MENU_TYPE_SUBMENU, "Counter Suite");
}
} // namespace CounterSuite
