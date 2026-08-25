#include "main_menu.h"
#include "display.h"
#include "utils.h"
#include <globals.h>

namespace {
struct MenuNameAlias {
    const char *stableName;
    const char *legacyDisplayName;
};

const MenuNameAlias legacyMenuNames[] = {
    {"BLE",            "Bluetooth"     },
    {"Clock",          "Relógio"        },
    {"Config",         "Configurações"  },
    {"Connect",        "Conexões"        },
    {"Ethernet",       "Ethernet"      },
    {"Files",          "Arquivos"      },
    {"FM",             "Rádio FM"       },
    {"GPS",            "GPS"           },
    {"IR",             "Infravermelho" },
    {"LoRa",           "LoRa"          },
    {"NRF24",          "nRF24"         },
    {"Others",         "Ferramentas"   },
    {"RFID",           "NFC / RFID"    },
    {"RF",             "Sub-GHz"       },
    {"JS Interpreter", "Scripts em Js" },
    {"WiFi",           "Rede"          },
};

bool matchesMenuName(const String &storedName, const MenuItemInterface *item) {
    if (storedName.equalsIgnoreCase(item->getName()) || storedName.equalsIgnoreCase(item->getDisplayName())) {
        return true;
    }

    for (const auto &alias : legacyMenuNames) {
        if (item->getName().equalsIgnoreCase(alias.stableName) &&
            storedName.equalsIgnoreCase(alias.legacyDisplayName)) {
            return true;
        }
    }
    return false;
}

bool migrateDisabledMenuNames(const std::vector<MenuItemInterface *> &items) {
    std::vector<String> migratedNames;
    migratedNames.reserve(bruceConfig.disabledMenus.size());
    bool changed = false;

    for (const String &storedName : bruceConfig.disabledMenus) {
        String resolvedName = storedName;
        for (const auto *item : items) {
            if (matchesMenuName(storedName, item)) {
                resolvedName = item->getName();
                break;
            }
        }

        if (resolvedName != storedName) changed = true;
        if (std::find(migratedNames.begin(), migratedNames.end(), resolvedName) == migratedNames.end()) {
            migratedNames.push_back(resolvedName);
        } else {
            changed = true;
        }
    }

    if (changed) bruceConfig.disabledMenus = migratedNames;
    return changed;
}
} // namespace

MainMenu::MainMenu() {
    _menuItems = {
        &wifiMenu,
        &bleMenu,
        &rfMenu,
        &nrf24Menu,
#if !defined(LITE_VERSION)
        &loraMenu,
#endif
#if defined(FM_SI4713) && !defined(LITE_VERSION)
        &fmMenu,
#endif
        &irMenu,
#if !defined(LITE_VERSION)
        &ethernetMenu,
#endif
        &gpsMenu,
        &rfidMenu,
        &fileMenu,
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
        &scriptsMenu,
#endif
        &clockMenu,
        &othersMenu,
        &configMenu,
        &maliToolsMenu,
        &maliCounterMenu,
    };

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

void MainMenu::begin(void) {
    returnToMenu = false;
    options = {};

    if (migrateDisabledMenuNames(_menuItems)) bruceConfig.saveFile();

    std::vector<String> l = bruceConfig.disabledMenus;
    for (int i = 0; i < _totalItems; i++) {
        String stableName = _menuItems[i]->getName();
        String displayName = _menuItems[i]->getDisplayName();
        if (find(l.begin(), l.end(), stableName) == l.end()) { // If menu item is not disabled
            options.push_back(
                {// selected lambda
                 displayName,
                 [this, i]() { _menuItems[i]->optionsMenu(); },
                 false,                                  // selected = false
                 [](void *menuItem, bool shouldRender) { // render lambda
                     if (!shouldRender) return false;
                     drawMainBorder(false);

                     MenuItemInterface *obj = static_cast<MenuItemInterface *>(menuItem);
                     float scale = float((float)tftWidth / (float)240);
                     if (bruceConfigPins.rotation & 0b01) scale = float((float)tftHeight / (float)135);
                     obj->draw(scale);
#if defined(HAS_TOUCH)
                     TouchFooter();
#endif
                     return true;
                 },
                 _menuItems[i]
                }
            );
        }
    }
    _currentIndex = loopOptions(options, MENU_TYPE_MAIN, "Menu Principal", _currentIndex);
};

/*********************************************************************
**  Function: hideAppsMenu
**  Menu to Hide or show menus
**********************************************************************/

void MainMenu::hideAppsMenu() {
    auto items = this->getItems();
    int index = 0;
RESTART: // using gotos to avoid stackoverflow after many choices
    options.clear();
    for (auto item : items) {
        String stableName = item->getName();
        String displayName = item->getDisplayName();
        std::vector<String> l = bruceConfig.disabledMenus;
        bool enabled = find(l.begin(), l.end(), stableName) == l.end();
        options.push_back(
            {displayName,
             [this, stableName, enabled]() {
                 if (enabled) bruceConfig.addDisabledMenu(stableName);
                 else bruceConfig.removeDisabledMenu(stableName);
             },
             enabled}
        );
    }
    options.push_back({"Mostrar todos", [=]() { bruceConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    index = loopOptions(options, index);
    bruceConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}
