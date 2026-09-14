#include "ui/MenuRoute.h"
#include "core/ui/PtBr.h"
#include "ui/MaliUI.h"
#include "mali_tools/counter/CounterLab.h"
#include "main_menu.h"
#include "display.h"
#include "led_control.h"
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
        &connectMenu,
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
    setLedState(MaliLedState::MENU);
    returnToMenu = false;
    options = {};

    if (migrateDisabledMenuNames(_menuItems)) bruceConfig.saveFile();

    std::vector<Option> rootOptions = {
        {MaliText::network_6845c6, [this](){openCategory(0);}},
        {"RADIO", [this](){openCategory(1);}},
        {MaliText::tools_9d0e51, [this](){openCategory(2);}},
        {"COUNTER", [this](){openCategory(3);}},
        {MaliText::files_9fb4f1, [this](){fileMenu.optionsMenu();}},
        {MaliText::system_29d437, [this](){openCategory(5);}},
    };
    rootOptions[4].enabled=std::find(bruceConfig.disabledMenus.begin(),bruceConfig.disabledMenus.end(),fileMenu.getName())==bruceConfig.disabledMenus.end();
    _currentIndex = loopOptions(rootOptions,MENU_TYPE_MAIN,"OS",_currentIndex);
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

void MainMenu::openCategory(int category) {
    int cursor = 0;
    while (!returnToMenu) {
        bool done = false;
        std::vector<Option> list;
        if (category == MaliUI::Counter) list.push_back({"Testes de resiliencia", CounterLab::open});
        for (auto *item : _menuItems) {
            const String name = item->getName();
            const bool enabled = std::find(bruceConfig.disabledMenus.begin(), bruceConfig.disabledMenus.end(), name) == bruceConfig.disabledMenus.end();
            if (enabled && MaliUI::categoryFor(name.c_str()) == category)
                list.push_back({item->getDisplayName(), [item]() { item->optionsMenu(); }});
        }
        list.push_back({"Voltar", [&]() { done = true; }});
        const char *titles[] = {"REDE", "RADIO", "FERRAMENTAS", "COUNTER", "ARQUIVOS", "SISTEMA"};
        cursor = loopOptions(list, MENU_TYPE_GEAR, titles[category], cursor);
        if (cursor < 0 || done) break;
    }
}
