#include "core/ui/MaliUI.h"
#include "KeyGauge.h"
#include "KeyGaugeStore.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include <cstdlib>
#include <cstring>
namespace KeyGauge {
namespace {
const char *directory = "/MaliTools/KeyGauge";
FS *storage = nullptr;
portMUX_TYPE previewMux = portMUX_INITIALIZER_UNLOCKED;
KeyGaugeProfile webPreview;
bool hasWebPreview = false, previewPending = false;
bool copyWebPreview(KeyGaugeProfile &profile, bool pendingOnly) {
    portENTER_CRITICAL(&previewMux);
    bool available = hasWebPreview && (!pendingOnly || previewPending);
    if (available) { profile = webPreview; previewPending = false; }
    portEXIT_CRITICAL(&previewMux);
    return available;
}
int calibration = 100; // Relative reference-bar scale, never a physical conversion.
int bounded(int n, int lo, int hi) { return n < lo ? lo : (n > hi ? hi : n); }
void label(int y, const String &text) {
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(text, 6, y, 1);
}
void draw(const KeyGaugeProfile &p, int selected, bool guide, const String &mode) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setTextDatum(0);
    if (!guide) {
        MaliUI::drawHeader("KEY GAUGE");
        label(29, "POINT " + String(selected + 1) + "/" + String(p.points) + " LEVEL: " + String(p.gaugePoints[selected].level) + " MODE: " + mode);
        label(41, "THICKNESS: " + String(p.thickness) + "  WIDTH: " + String(p.width) + "%");
    }
    const int top = guide ? 12 : 60;
    const int bottom = tftHeight - (guide ? 18 : 40);
    const int height = max(20, bottom - top);
    const int depth = height / 2;
    // The upper contour is independent of thickness. Only the lower body edge moves.
    const int base = top + depth + 2 + (height - depth - 2) * (p.thickness - 1) / 9;
    const int span = (tftWidth - 12) * p.width / 100;
    const int left = (tftWidth - span) / 2;
    const int start = left + span / 9, end = left + span - span / 20;
    int x[10], y[10];
    for (int i = 0; i < p.points; ++i) {
        x[i] = start + (end - start) * i / (p.points - 1);
        y[i] = top + depth * p.gaugePoints[i].level / 9;
    }
    if (!guide) {
        tft.drawRoundRect(left, top, max(4, start - left), base - top + 1, 3, bruceConfig.secColor);
        for (int i = 1; i < p.points; ++i) {
            for (int px = x[i-1]; px <= x[i]; ++px) {
                int py = y[i-1] + (y[i] - y[i-1]) * (px - x[i-1]) / max(1, x[i] - x[i-1]);
                tft.drawFastVLine(px, py, base - py + 1, bruceConfig.secColor);
            }
        }
    }
    tft.drawFastHLine(start, base, end - start + 1, bruceConfig.secColor);
    for (int i = 0; i < p.points; ++i) {
        tft.drawFastVLine(x[i], top, base - top + 1, i == selected ? bruceConfig.priColor : bruceConfig.secColor);
        if (i) tft.drawLine(x[i-1], y[i-1], x[i], y[i], TFT_LIGHTGREY);
        tft.fillCircle(x[i], y[i], i == selected ? 3 : 1, i == selected ? bruceConfig.priColor : TFT_LIGHTGREY);
    }
    // A separate pointer remains clear when markers meet the filled body.
    tft.fillTriangle(x[selected] - 4, top - 6, x[selected] + 4, top - 6,
                     x[selected], top - 2, bruceConfig.priColor);
    if (!guide) {
        String levels;
        for (int i = 0; i < p.points; ++i) levels += "P" + String(i+1) + ":" + String(p.gaugePoints[i].level) + " ";
        label(tftHeight - 33, levels);
        label(tftHeight - 22, mode == "VIEW" ? "OK:options BACK:return" :
            mode == "WEB / RAM" ? "WEB PREVIEW / RAM  BACK:WebUI" : "ENC:adjust OK:next BACK:options");
    }
    label(tftHeight - 11, guide ? "P" + String(selected + 1) + " L:" + String(p.gaugePoints[selected].level) + "  REFERENCE ONLY" : "REFERENCE ONLY");
}
bool number(const String &s, int lo, int hi, int &result) {
    if (!s.length()) return false;
    for (unsigned i = 0; i < s.length(); ++i) if (s[i] < '0' || s[i] > '9') return false;
    long value = strtol(s.c_str(), nullptr, 10);
    if (value < lo || value > hi) return false;
    result = value;
    return true;
}
void save(KeyGaugeProfile &p) {
    std::vector<String> names; String proposed;
    if (listProfiles(names, proposed) != 200) { displayError("Storage unavailable", true); return; }
    String name = keyboard(proposed, 31, "SAVE AS");
    if (returnToMenu || !name.length() || name == "\x1B") return;
    if (!validName(name)) { displayError("Use letters, digits, _ or -", true); return; }
    KeyGaugeProfile candidate = p;
    name.toCharArray(candidate.name, sizeof(candidate.name));
    int result = writeProfile(candidate, false);
    if (result != 200) { displayError(result == 409 ? "Name already exists" : "Save failed", true); return; }
    p = candidate;
    displayInfo("Saved: " + name, true);
}
void adjust(KeyGaugeProfile &p, bool width) {
    check(SelPress);
    const uint32_t entered = millis();
    bool dirty = true;
    while (!returnToMenu) {
        if (dirty) { draw(p, 0, false, width ? "PROFILE WIDTH" : "THICKNESS"); label(tftHeight - 22, "ENC:adjust OK:confirm BACK:return"); dirty = false; }
        if (check(EscPress)) break;
        if (check(SelPress) && millis() - entered > 250) break;
        int delta = check(NextPress) ? 1 : (check(PrevPress) ? -1 : 0);
        if (delta) {
            if (width) p.width = bounded(p.width + delta, 50, 100);
            else p.thickness = bounded(p.thickness + delta, 1, 10);
            dirty = true;
        }
        delay(1);
    }
}
bool choosePoints(KeyGaugeProfile &p) {
    std::vector<Option> options;
    for (int n = 4; n <= 10; ++n) options.push_back({String(n).c_str(), [&p, n]() { p.points = n; }});
    return loopOptions(options, MENU_TYPE_SUBMENU, "NUMBER OF POINTS", p.points - 4) >= 0;
}
void edit(KeyGaugeProfile &p, bool guide = false) {
    int selected = 0; bool done = false, dirty = true;
    uint32_t entered = millis();
    check(SelPress);
    while (!done && !returnToMenu) {
        if (dirty) { draw(p, selected, guide, "EDIT"); dirty = false; }
        if (check(EscPress)) {
            std::vector<Option> options = {
                {"POINTS", [&]() { choosePoints(p); selected = min(selected, int(p.points)-1); }},
                {"THICKNESS", [&]() { adjust(p, false); }},
                {"PROFILE", [&]() { guide = false; }},
                {"GUIDE", [&]() { guide = true; }},
                {"PROFILE WIDTH", [&]() { adjust(p, true); }},
                {"PREVIOUS POINT", [&]() { selected = (selected + p.points - 1) % p.points; }},
                {"SAVE AS", [&]() { save(p); }},
                {"BACK TO EDIT", []() {}},
                {"EXIT", [&]() { done = true; }}
            };
            loopOptions(options, MENU_TYPE_SUBMENU, "KEY GAUGE / EDIT"); dirty = true;
            entered = millis();
        }
        int delta = check(NextPress) ? 1 : (check(PrevPress) ? -1 : 0);
        if (delta) { p.gaugePoints[selected].level = bounded(p.gaugePoints[selected].level + delta, 0, 9); dirty = true; }
        if (check(SelPress) && millis() - entered > 250) { selected = (selected + 1) % p.points; dirty = true; }
        delay(1);
    }
}
void removeProfile(const String &path) {
    std::vector<Option> options = {{"CANCEL", []() {}}, {"DELETE", [path]() { if (deleteProfile(path) != 200) displayError("Delete failed", true); }}};
    loopOptions(options, MENU_TYPE_SUBMENU, "DELETE PROFILE?");
}
void browse(bool deleting) {
    std::vector<String> names; String nextName;
    if (listProfiles(names, nextName) != 200) { displayError("Storage unavailable", true); return; }
    std::vector<Option> options;
    for (const String &name : names) {
        {
            String path = name;
            options.push_back({name.c_str(), [path, deleting]() {
                if (deleting) { removeProfile(path); return; }
                KeyGaugeProfile p;
                if (readProfile(path, p) != 200) { displayError("Invalid profile", true); return; }
                bool back = false;
                while (!back && !returnToMenu) {
                    draw(p, 0, false, "VIEW");
                    const uint32_t entered = millis();
                    bool exitView = false;
                    while (!returnToMenu) {
                        if (check(EscPress)) { exitView = true; break; }
                        if (check(SelPress) && millis() - entered > 250) break;
                        delay(1);
                    }
                    if (exitView) break;
                    if (returnToMenu) break;
                    std::vector<Option> view = {
                        {"EDIT", [&]() { edit(p); }}, {"GUIDE", [&]() { edit(p, true); }},
                        {"DELETE", [&]() { removeProfile(path); KeyGaugeProfile checkProfile; back = readProfile(path, checkProfile) == 404; }},
                        {"BACK", [&]() { back = true; }}
                    };
                    if (loopOptions(view, MENU_TYPE_SUBMENU, p.name) < 0) back = true;
                }
            }});
        }
    }
    options.push_back({"BACK", []() {}});
    loopOptions(options, MENU_TYPE_SUBMENU, deleting ? "DELETE PROFILE" : "LOAD PROFILE");
}
void calibrate() {
    int value = calibration; bool dirty = true;
    check(SelPress);
    while (!returnToMenu) {
        if (dirty) {
            tft.fillScreen(bruceConfig.bgColor); tft.setTextSize(1);
            label(8, "CALIBRATION / 10.0 mm reference");
            int span = (tftWidth - 20) * value / 200, x = (tftWidth-span)/2, y = tftHeight/2;
            tft.drawFastHLine(x, y, span, bruceConfig.priColor);
            tft.drawFastVLine(x, y-5, 11, bruceConfig.priColor);
            tft.drawFastVLine(x+span, y-5, 11, bruceConfig.priColor);
            label(tftHeight-30, "ENC:scale OK:save BACK:cancel"); label(tftHeight-15, "REFERENCE ONLY"); dirty = false;
        }
        if (check(EscPress)) return;
        if (check(SelPress)) {
            File f = storage->open(String(directory) + "/calibration.txt", FILE_WRITE);
            if (!f || f.print(String(value)) == 0) displayError("Save failed", true);
            else calibration = value;
            return;
        }
        int delta = check(NextPress) ? 1 : (check(PrevPress) ? -1 : 0);
        if (delta) { value = bounded(value + delta, 20, 200); dirty = true; }
        delay(1);
    }
}
}
void queueWebPreview(const KeyGaugeProfile &profile) {
    if (!validProfile(profile)) return;
    portENTER_CRITICAL(&previewMux);
    webPreview = profile; hasWebPreview = true; previewPending = true;
    portEXIT_CRITICAL(&previewMux);
}
bool processWebPreview() {
    KeyGaugeProfile profile;
    if (!copyWebPreview(profile, true)) return false;
    draw(profile, 0, false, "WEB / RAM");
    return true;
}
void open() {
    if (!getFsStorage(storage)) return;
    if ((!storage->exists("/MaliTools") && !storage->mkdir("/MaliTools")) ||
        (!storage->exists(directory) && !storage->mkdir(directory))) { displayError("Storage unavailable", true); return; }
    File f = storage->open(String(directory) + "/calibration.txt", FILE_READ);
    if (f && f.size() <= 3) { int value; if (number(f.readString(), 20, 200, value)) calibration = value; }
    f.close();
    bool done = false;
    while (!done && !returnToMenu) {
        std::vector<Option> options = {
            {"NEW PROFILE", []() { KeyGaugeProfile p; if (choosePoints(p) && !returnToMenu) edit(p); }},
            {"LOAD PROFILE", []() { browse(false); }}, {"DELETE PROFILE", []() { browse(true); }},
            {"WEB PREVIEW", []() {
                KeyGaugeProfile p;
                if (copyWebPreview(p, false)) edit(p);
                else displayInfo("No web preview received", true);
            }},
            {"CALIBRATION", calibrate},
            {"ABOUT", []() { displayInfo("KEY GAUGE\nGeometric profile tool\nManual profile visualization\nfor educational use and\nowned objects.\nReference measurements only.", true); }},
            {"EXIT", [&]() { done = true; }}
        };
        if (loopOptions(options, MENU_TYPE_SUBMENU, "MALI / KEY GAUGE") < 0) break;
    }
}
}
