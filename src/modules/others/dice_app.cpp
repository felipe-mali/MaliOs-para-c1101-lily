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
    drawMainBorderWithTitle("D" + String(sides));

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.drawCentreString("Resultado", tftWidth / 3, 38, 1);

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(4 * FP);
    tft.drawCentreString(String(result), tftWidth / 3, 55, 1);
    tft.setTextSize(FP);

    const int historyX = (2 * tftWidth) / 3;
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Ultimas", historyX, 40, 1);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    for (uint8_t i = 0; i < historyCount; ++i) {
        tft.drawCentreString(String(history[i]), historyX, 57 + i * (LH + 4), 1);
    }

    if (finalResult && sides == 20 && (result == 1 || result == 20)) {
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawCentreString(result == 20 ? "CRITICO!" : "FALHA CRITICA!", tftWidth / 3, 102, 1);
    }

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("SEL rola | BACK volta", tftWidth / 2, tftHeight - LH - 3, 1);
}

bool animateRoll(int sides, int &result) {
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

        const int selected = loopOptions(diceOptions, MENU_TYPE_SUBMENU, "Dados");
        if (selected < 0 || leave) return;
        if (selectedSides > 0) runDie(selectedSides);
    }
}
