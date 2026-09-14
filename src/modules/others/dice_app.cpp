#include "core/ui/MaliUI.h"
#include "core/ui/PtBr.h"
#include "dice_app.h"

#include "core/display.h"
#include <esp_system.h>

namespace {
constexpr uint8_t HISTORY_SIZE = 3;

int lastSides = 20;
int history[HISTORY_SIZE] = {0, 0, 0};
uint8_t historyCount = 0;

uint32_t randomBelow(uint32_t upperBound) {
    if (upperBound < 2) return 0;

    const uint32_t rejectionLimit = UINT32_MAX - (UINT32_MAX % upperBound);
    uint32_t value;
    do { value = esp_random(); } while (value >= rejectionLimit);
    return value % upperBound;
}

int rollDie(int sides) { return static_cast<int>(randomBelow(static_cast<uint32_t>(sides))) + 1; }

void rememberRoll(int result) {
    for (int i = HISTORY_SIZE - 1; i > 0; --i) history[i] = history[i - 1];
    history[0] = result;
    if (historyCount < HISTORY_SIZE) ++historyCount;
}

void drawRollScreen(int sides, int result, bool finalResult) {
    const bool portrait = tftHeight > tftWidth;
    const int resultW = portrait ? tftWidth - 16 : tftWidth * 3 / 5 - 12;
    const int resultH = portrait ? 136 : tftHeight - 56;
    const int historyX = portrait ? 8 : resultW + 16;
    const int historyY = portrait ? 178 : 32;
    const int historyW = portrait ? tftWidth - 16 : tftWidth - historyX - 8;
    MaliUI::drawCard(8, 32, resultW, resultH, true);
    MaliUI::drawCard(historyX, historyY, historyW, portrait ? tftHeight - 202 : resultH);
    tft.setTextDatum(0);
    tft.setTextSize(1);
    tft.setTextColor(MaliUI::TEXT_SECONDARY, MaliUI::SURFACE);
    tft.drawCentreString("Resultado", 8 + resultW / 2, 42, 1);
    tft.drawCentreString("Ultimas", historyX + historyW / 2, historyY + 10, 1);
    tft.setTextSize(4);
    tft.setTextColor(MaliUI::TEXT_PRIMARY, MaliUI::SURFACE);
    tft.drawCentreString(String(result), 8 + resultW / 2, 64, 1);
    tft.setTextSize(1);
    for (uint8_t i = 0; i < historyCount; ++i)
        tft.drawCentreString(String(history[i]), historyX + historyW / 2, historyY + 28 + i * 16, 1);
    if (finalResult && sides == 20 && (result == 1 || result == 20)) {
        tft.setTextColor(result == 20 ? MaliUI::SUCCESS : MaliUI::ERROR, MaliUI::SURFACE);
        tft.drawCentreString(result == 20 ? "CRITICO!" : "FALHA CRITICA!", 8 + resultW / 2, 32 + resultH - 18, 1);
    }
    MaliUI::drawFooter("SEL: rolar  VOLTAR: menu");
}

bool animateRoll(int sides, int &result) {
    tft.fillScreen(MaliUI::BACKGROUND);
    MaliUI::drawHeader("D" + String(sides));
    for (uint8_t frame = 0; frame < 6; ++frame) {
        result = rollDie(sides);
        drawRollScreen(sides, result, false);
        if (check(EscPress)) return false;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    return true;
}

void runDie(int sides) {
    lastSides = sides;
    int result = 1;

    if (!animateRoll(sides, result)) return;
    rememberRoll(result);
    drawRollScreen(sides, result, true);

    while (true) {
        if (check(EscPress)) return;
        if (check(SelPress)) {
            if (!animateRoll(sides, result)) return;
            rememberRoll(result);
            drawRollScreen(sides, result, true);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace

void dice_app() {
    while (true) {
        int selectedSides = 0;
        bool leave = false;
        const String quickLabel = "Rolagem rapida (D" + String(lastSides) + ")";
        std::vector<Option> diceOptions = {
            {quickLabel, [&]() { selectedSides = lastSides; }},
            {"D4", [&]() { selectedSides = 4; }},
            {"D6", [&]() { selectedSides = 6; }},
            {"D8", [&]() { selectedSides = 8; }},
            {"D10", [&]() { selectedSides = 10; }},
            {"D12", [&]() { selectedSides = 12; }},
            {"D20", [&]() { selectedSides = 20; }},
            {"D100", [&]() { selectedSides = 100; }},
            {"Voltar", [&]() { leave = true; }},
        };

        const int selected = loopOptions(diceOptions, MENU_TYPE_GEAR, "Dados");
        if (selected < 0 || leave) return;
        if (selectedSides > 0) runDie(selectedSides);
    }
}
