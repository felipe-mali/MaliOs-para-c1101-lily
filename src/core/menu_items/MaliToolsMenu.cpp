#include "MaliToolsMenu.h"

#include "core/main_menu.h"
#include "core/utils.h"
#include "modules/mali/MaliSystemInfo.h"

void MaliToolsMenu::optionsMenu() {
    returnToMenu = false;
    while (!returnToMenu) {
        std::vector<Option> maliOptions = {
            {"Info. do Sistema", MaliSystemInfo::showSystemInfo   },
            {"Status do Hardware", MaliSystemInfo::showHardwareStatus},
            {"Energia", MaliSystemInfo::showEnergy               },
            {"Acesso Rapido", [this]() { quickAccessMenu(); }     },
            {"Sobre o MaliOS", MaliSystemInfo::showAbout          },
            {"Menu Principal", backToMenu                         },
        };

        int selected = loopOptions(maliOptions, MENU_TYPE_SUBMENU, "Mali Tools");
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

    loopOptions(quickOptions, MENU_TYPE_SUBMENU, "Acesso Rapido");
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
