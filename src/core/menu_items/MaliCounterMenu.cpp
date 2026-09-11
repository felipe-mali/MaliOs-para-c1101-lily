#include "mali_tools/counter/counter_main.h"
#include "MaliCounterMenu.h"

#include "core/utils.h"
#include "modules/mali/MaliCounter.h"
#include "modules/mali/MaliWiki.h"

void MaliCounterMenu::optionsMenu() {
    std::vector<Option> counterOptions = {
        {"Counter Suite", CounterSuite::open},
        {"Sobre a contagem", []() {
             displayInfo("Atividade passiva; eventos de radio nao equivalem a dispositivos.", true);
         }},
        {"Menu principal", backToMenu},
        {"? Ajuda", []() { MaliWiki::open(MaliWiki::Category::MALI_COUNTER); }},
    };

    loopOptions(counterOptions, MENU_TYPE_SUBMENU, "Mali Counter");
}

void MaliCounterMenu::drawIcon(float scale) {
    clearIconArea();

    const int width = max(2, static_cast<int>(scale * 3));
    const int spacing = max(8, static_cast<int>(scale * 13));
    const int baseY = iconCenterY + spacing * 2;
    const int heights[] = {spacing, spacing * 2, spacing * 3, spacing * 4};

    for (int i = 0; i < 4; ++i) {
        int x = iconCenterX - spacing * 2 + i * spacing;
        uint16_t color = i % 2 == 0 ? bruceConfig.priColor : bruceConfig.secColor;
        tft.fillRoundRect(x, baseY - heights[i], width * 2, heights[i], width, color);
    }
}
