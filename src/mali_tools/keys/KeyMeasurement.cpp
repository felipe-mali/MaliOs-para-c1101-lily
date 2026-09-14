#include "KeyMeasurement.h"
#include "KeyInput.h"
#include "core/display.h"
#include "core/scrollableTextArea.h"
#include "core/ui/KeysPtBr.h"
#include <cstring>
namespace MaliKeys {
namespace T = MaliText::Keys;
String millimetres(Measure value, bool unit) {
    char text[24]; snprintf(text, sizeof(text), "%u,%02u%s", unsigned(value/100), unsigned(value%100), unit ? " mm" : "");
    return text;
}
String difference(Measure a, Measure b) {
    if (!a || !b) return T::Unknown;
    int delta = int(a) - int(b);
    return String(delta < 0 ? "-" : "+") + millimetres(Measure(delta < 0 ? -delta : delta));
}
bool editNumber(const char *title, int &value, int lo, int hi, int step, bool mm) {
    int current = value; bool dirty = true;
    tft.fillScreen(MaliUI::BACKGROUND); MaliUI::drawHeader(title);
    MaliUI::drawFooter(T::ValueNav);
    KeyInput input;
    while (!returnToMenu) {
        if (dirty) {
            MaliUI::drawCard(8, 36, tftWidth-16, tftHeight-64, true);
            tft.setTextDatum(0); tft.setTextSize(2); tft.setTextColor(MaliUI::TEXT_PRIMARY, MaliUI::SURFACE);
            tft.drawCentreString(mm ? millimetres(current) : String(current), tftWidth/2, tftHeight/2-18, 1);
            tft.setTextSize(1); tft.setTextColor(MaliUI::TEXT_SECONDARY, MaliUI::SURFACE);
            tft.drawCentreString(T::CancelHold, tftWidth/2, tftHeight-52, 1);
            if (mm && current == 0) tft.drawCentreString(T::Unknown, tftWidth/2, tftHeight/2+4, 1);
            dirty = false;
        }
        auto e = input.read();
        if (e.back) return false;
        if (e.select) { value = current; return true; }
        if (e.steps) {
            int next = adjusted(current, e.steps, step, lo, hi);
            dirty = next != current; current = next;
        }
        delay(5);
    }
    return false;
}
bool editText(const char *title, char *buffer, size_t size) {
    if (size < 2 || size > sizeof(KeyProfile::notes)) return false;
    // Small local editor: all input paths support release-to-select and hold-to-cancel.
    char text[sizeof(KeyProfile::notes)] = {};
    strlcpy(text, buffer, size);
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.,:/()+@!?";
    constexpr int count = sizeof(alphabet)-1+3;
    int selected = 3; bool dirty = true;
    tft.fillScreen(MaliUI::BACKGROUND); MaliUI::drawHeader(title); MaliUI::drawFooter(T::TextNav);
    KeyInput input;
    while (!returnToMenu) {
        if (dirty) {
            size_t len = strlen(text);
            MaliUI::drawCard(8, 34, tftWidth-16, tftHeight-60);
            tft.setTextDatum(0); tft.setTextSize(1); tft.setTextColor(MaliUI::TEXT_PRIMARY, MaliUI::SURFACE);
            int chars = max(1, (tftWidth-32)/6);
            String visible(text + (len > size_t(chars*2) ? len-chars*2 : 0));
            tft.drawString(visible.substring(0, chars), 16, 44, 1);
            tft.drawString(visible.substring(chars), 16, 56, 1);
            tft.setTextColor(MaliUI::TEXT_SECONDARY, MaliUI::SURFACE);
            tft.drawString(String(len)+"/"+String(size-1), 16, 70, 1);
            String label = selected==0 ? T::FinishText : selected==1 ? T::Erase : selected==2 ? T::Space : String(alphabet[selected-3]);
            int y = max(90, tftHeight/2);
            MaliUI::drawRoundedFill(18, y, tftWidth-36, 34, MaliUI::SURFACE_ALT);
            tft.setTextSize(selected>=3 ? 3 : 1); tft.setTextColor(MaliUI::ACCENT, MaliUI::SURFACE_ALT);
            tft.drawCentreString(label, tftWidth/2, y+6, 1);
            tft.setTextSize(1);
            dirty = false;
        }
        auto e = input.read();
        if (e.back) return false;
        if (e.steps) { selected = MaliUI::wrap(int64_t(selected)+e.steps, count); dirty = true; }
        if (e.select) {
            size_t len = strlen(text);
            if (selected==0) { strlcpy(buffer, text, size); return true; }
            if (selected==1 && len) text[len-1] = 0;
            if (selected>=2 && len<size-1) { text[len] = selected==2 ? ' ' : alphabet[selected-3]; text[len+1] = 0; }
            dirty = true;
        }
        delay(5);
    }
    return false;
}
void showText(const char *title, const String &text) {
    tft.fillScreen(MaliUI::BACKGROUND); MaliUI::drawHeader(title); MaliUI::drawFooter(T::Nav);
    ScrollableTextArea area(1, 8, 34, tftWidth-16, tftHeight-60, false);
    area.fromString(text); area.draw(true);
    KeyInput input;
    while (!returnToMenu) {
        auto e = input.read(); if (e.back || e.select) return;
        if (e.steps) {
            int64_t line = int64_t(area.firstVisibleLine) + e.steps;
            int maxLine = max(0, int(area.getMaxLines())-(tftHeight-60)/10);
            area.scrollToLine(size_t(line < 0 ? 0 : line > maxLine ? maxLine : line));
            area.draw(true);
        }
        delay(5);
    }
}
}
