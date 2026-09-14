#include "core/ui/PtBr.h"
#include "modules/others/pixel_paint_app.h"
#include "modules/others/dice_app.h"
#include "mali_tools/counter/counter_main.h"
#include "mali_tools/counter/CounterLab.h"
#include "mali_tools/key_gauge/KeyGauge.h"
#include "mali_tools/keys/MaliKeys.h"
#include "core/ui/KeysPtBr.h"
#include "MaliToolsMenu.h"

#include "core/main_menu.h"
#include "core/utils.h"
#include "modules/mali/MaliQrMenu.h"
#include "modules/mali/MaliSystemInfo.h"
#include "modules/mali/MaliWiki.h"

void MaliToolsMenu::optionsMenu() {
    returnToMenu = false;
    while (!returnToMenu) {
        std::vector<Option> maliOptions = {
            {"PIXEL PAINT", pixel_paint_app},
            {"D20", dice_app},
            {"COUNTER", CounterLab::open},
            {MaliText::utilities_17562b, [](){mainMenu.othersMenu.optionsMenu();}},
            {"Counter Suite", CounterSuite::open},
            {MaliText::Keys::Title, MaliKeys::open},
            {"KEY GAUGE", KeyGauge::open},
            {"Info. do Sistema", MaliSystemInfo::showSystemInfo   },
            {"Status do Hardware", MaliSystemInfo::showHardwareStatus},
            {"Energia", MaliSystemInfo::showEnergy               },
            {"QR", MaliQrMenu::open                              },
            {"Acesso Rapido", [this]() { quickAccessMenu(); }     },
            {"Sobre o MaliOS", MaliSystemInfo::showAbout          },
            {"Menu Principal", backToMenu                         },
            {"? Ajuda", []() { MaliWiki::open(MaliWiki::Category::MALI_TOOLS); }},
        };

        int selected = loopOptions(maliOptions, MENU_TYPE_GEAR, MaliText::tools_9d0e51);
        if (selected < 0) break;
    }
}

void MaliToolsMenu::quickAccessMenu() {
    std::vector<Option> quickOptions = {
        {"Rede", []() { mainMenu.wifiMenu.optionsMenu(); }     },
        {"Bluetooth", []() { mainMenu.bleMenu.optionsMenu(); } },
        {"Sub-GHz", []() { mainMenu.rfMenu.optionsMenu(); }    },
        {"nRF24", []() { mainMenu.nrf24Menu.optionsMenu(); }   },
        {"NFC / RFID", []() { mainMenu.rfidMenu.optionsMenu(); }},
        {"Infravermelho", []() { mainMenu.irMenu.optionsMenu(); }},
        {"Arquivos", []() { mainMenu.fileMenu.optionsMenu(); } },
        {"Configuracoes", []() { mainMenu.configMenu.optionsMenu(); }},
        {"Voltar", []() {}                                      },
    };

    loopOptions(quickOptions, MENU_TYPE_GEAR, "Acesso Rapido");
}

void MaliToolsMenu::drawIcon(float scale) {
    clearIconArea();

    int radius = max(4, static_cast<int>(scale * 7));
    int arm = max(12, static_cast<int>(scale * 24));
    int width = max(2, static_cast<int>(scale * 4));

    tft.drawWideLine(
        iconCenterX - arm,
        iconCenterY + arm,
        iconCenterX + arm,
        iconCenterY - arm,
        width,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawCircle(iconCenterX - arm, iconCenterY + arm, radius, bruceConfig.secColor);
    tft.fillCircle(iconCenterX + arm, iconCenterY - arm, radius, bruceConfig.priColor);
    tft.drawCircle(iconCenterX, iconCenterY, radius * 2, bruceConfig.secColor);
    tft.fillCircle(iconCenterX, iconCenterY, radius, bruceConfig.bgColor);
}
