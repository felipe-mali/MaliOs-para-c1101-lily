#include "counter_events.h"
#include "core/display.h"
#include "core/led_control.h"
#include "core/sd_functions.h"
#include "core/utils.h"
#include <globals.h>
namespace CounterSuite {
namespace {
struct Entry {
    uint32_t sequence = 0;
    char text[128] = {};
};
Entry entries[64];
uint32_t total = 0, persisted = 0;
uint8_t used = 0;
} // namespace
uint32_t eventCount() { return total; }
void event(Module module, const String &message, bool warning) {
    Entry &e = entries[total % 64];
    e.sequence = ++total;
    used = min(64, int(used) + 1);
    uint32_t seconds = millis() / 1000;
    snprintf(
        e.text,
        sizeof(e.text),
        "[up %02lu:%02lu:%02lu] %s %s",
        (unsigned long)(seconds / 3600),
        (unsigned long)((seconds / 60) % 60),
        (unsigned long)(seconds % 60),
        name(module),
        message.c_str()
    );
    setLedState(
        warning ? MaliLedState::ERROR
                : (module == RF || module == NFC ? MaliLedState::COUNTER_SIGNAL : MaliLedState::SUCCESS)
    );
}
void flushLog() {
    if (persisted == total || !sdcardMounted) return;
    if (!SD.exists("/MaliCounter") && !SD.mkdir("/MaliCounter")) return;
    File f = SD.open("/MaliCounter/events.log", FILE_APPEND);
    if (!f) return;
    for (uint32_t i = total - used; i < total; ++i) {
        Entry &e = entries[i % 64];
        if (e.sequence > persisted) {
            if (!f.println(e.text)) break;
            persisted = e.sequence;
        }
    }
    f.close();
}
void logMenu() {
    std::vector<Option> menu = {
        {"View Log",
         []() {
             std::vector<Option> rows;
             for (uint32_t i = total - used; i < total; ++i) {
                 String line(entries[i % 64].text);
                 rows.push_back({line.c_str(), [line]() { displayInfo(line, true); }});
             }
             if (rows.empty()) rows.push_back({"Sem eventos", []() {}});
             loopOptions(rows, MENU_TYPE_SUBMENU, "Event Log (uptime)");
         }                    },
        {"Clear Log",
         []() {
             if (sdcardMounted && SD.exists("/MaliCounter/events.log") &&
                 !SD.remove("/MaliCounter/events.log")) {
                 displayError("Falha ao limpar SD", true);
                 return;
             }
             total = persisted = used = 0;
         }                    },
        {"Export Log",
         []() {
             if (!setupSdCard()) {
                 displayError("SD indisponivel", true);
                 return;
             }
             if (total == 0) {
                 SD.mkdir("/MaliCounter");
                 File empty = SD.open("/MaliCounter/events.log", FILE_APPEND);
                 if (!empty) {
                     displayError("Falha ao exportar", true);
                     return;
                 }
                 empty.close();
             }
             flushLog();
             if (persisted != total) displayError("Falha ao exportar", true);
             else displayInfo("/MaliCounter/events.log", true);
         }                    },
        {"Voltar",     []() {}}
    };
    loopOptions(menu, MENU_TYPE_SUBMENU, "Counter Log");
}
} // namespace CounterSuite
