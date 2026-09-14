#include "ConfigMenu.h"
#include "../mykeyboard.h"
#include "core/display.h"
#include "core/i2c_finder.h"
#include "core/main_menu.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#ifdef HAS_RGB_LED
#include "core/led_control.h"
#endif

/*********************************************************************
**  Function: optionsMenu
**  Main Config menu entry point
**********************************************************************/
void ConfigMenu::optionsMenu() {
    returnToMenu = false;
    while (true) {
        // Check if we need to exit to Main Menu (e.g., DevMode disabled)
        if (returnToMenu) {
            returnToMenu = false; // Reset flag
            return;
        }

        std::vector<Option> localOptions = {
            {"Tela e interface", [this]() { displayUIMenu(); }},
#ifdef HAS_RGB_LED
            {"Config. LED",      [this]() { ledMenu(); }      },
#endif
#if !defined(LITE_VERSION) && (defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR))
            {"Config. audio",    [this]() { audioMenu(); }    },
#endif
            {"Config. sistema",  [this]() { systemMenu(); }   },
            {"Energia",          [this]() { powerMenu(); }    },
        };

#if !defined(LITE_VERSION)
        if (!appStoreInstalled()) {
            localOptions.push_back({"Instalar App Store", []() { installAppStoreJS(); }});
        }
#endif

        if (bruceConfig.devMode) {
            localOptions.push_back({"Modo dev", [this]() { devMenu(); }});
        }

        localOptions.push_back({"Sobre", showDeviceInfo});
        localOptions.push_back({"Menu Principal", []() {}});

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Configuracoes");

        // Exit to Main Menu only if user pressed Back
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise rebuild Config menu after submenu returns
    }
}

/*********************************************************************
**  Function: displayUIMenu
**  Display & UI configuration submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::displayUIMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Brilho",            [this]() { setBrightnessMenu(); }               },
            {"Tempo de espera",   [this]() { setDimmerTimeMenu(); }               },
            {"Orientacao",        [this]() { lambdaHelper(gsetRotation, true)(); }},
            {"Cores da interface", [this]() { setUIColor(); }                      },
            {"Tema da interface", [this]() { setTheme(); }                        },
            {"Voltar",            []() {}                                         },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Tela e interface");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise loop continues and menu rebuilds
    }
}

/*********************************************************************
**  Function: ledMenu
**  LED configuration submenu with auto-rebuild for toggles
**********************************************************************/
#ifdef HAS_RGB_LED
void ConfigMenu::ledMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Cor do LED",
             [this]() {
                 beginLed();
                 setLedColorConfig();
             }                                                                            },
            {"Efeito do LED",
             [this]() {
                 beginLed();
                 setLedEffectConfig();
             }                                                                            },
            {"Brilho do LED",
             [this]() {
                 beginLed();
                 setLedBrightnessConfig();
             }                                                                            },
            {String("Piscar LED: ") + (bruceConfig.ledBlinkEnabled ? "LIG" : "DESL"),
             [this]() {
                 // Toggle LED blink setting
                 bruceConfig.ledBlinkEnabled = !bruceConfig.ledBlinkEnabled;
                 bruceConfig.saveFile();
             }                                                                            },
            {String("Efeitos de status: ") + (bruceConfig.ledStatusEffects ? "LIG" : "DESL"),
             [this]() {
                 bruceConfig.setLedStatusEffects(!bruceConfig.ledStatusEffects);
                 ledSetup();
             }                                                                            },
            {"Voltar",                                                             []() {}},
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Config. LED");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle label
    }
}
#endif
/*********************************************************************
**  Function: audioMenu
**  Audio configuration submenu with auto-rebuild for toggles
**********************************************************************/
void ConfigMenu::audioMenu() {
    while (true) {
        std::vector<Option> localOptions = {
#if !defined(LITE_VERSION)
#if defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR)

            {String("Som: ") + (bruceConfig.soundEnabled ? "LIG" : "DESL"),
                                                             [this]() {
                 // Toggle sound setting
                 bruceConfig.soundEnabled = !bruceConfig.soundEnabled;
                 bruceConfig.saveFile();
             }                                                                                                                                            },
#if defined(HAS_NS4168_SPKR)
            {"Volume",                                                      [this]() { setSoundVolume(); }},
#endif  // BUZZ_PIN || HAS_NS4168_SPKR
#endif  //  HAS_NS4168_SPKR
#endif  //  LITE_VERSION
            {"Voltar",                                                      []() {}                       },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Config. audio");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle label
    }
}

/*********************************************************************
**  Function: systemMenu
**  System configuration submenu with auto-rebuild for toggles
**********************************************************************/
void ConfigMenu::systemMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {String("InstaBoot: ") + (bruceConfig.instantBoot ? "LIG" : "DESL"),
             [this]() {
                 // Toggle InstaBoot setting
                 bruceConfig.instantBoot = !bruceConfig.instantBoot;
                 bruceConfig.saveFile();
             }                                                                                                           },
            {String("Wi-Fi ao iniciar: ") + (bruceConfig.wifiAtStartup ? "LIG" : "DESL"),
             [this]() {
                 // Toggle WiFi at startup setting
                 bruceConfig.wifiAtStartup = !bruceConfig.wifiAtStartup;
                 bruceConfig.saveFile();
             }                                                                                                           },
            {"App inicial",                                                         [this]() { setStartupApp(); }        },
            {"Ocultar/mostrar apps",                                                [this]() { mainMenu.hideAppsMenu(); }},
            {"Relogio",                                                             [this]() { setClock(); }             },
            {String("Idioma teclado: ") + bruceConfig.keyboardLang,                 [this]() { setKeyboardLanguage(); }  },
            {"Avancado",                                                            [this]() { advancedMenu(); }         },
            {"Voltar",                                                              []() {}                              },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Config. sistema");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle labels
    }
}

/*********************************************************************
**  Function: advancedMenu
**  Advanced settings submenu (nested under System Config)
**********************************************************************/
void ConfigMenu::advancedMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Definir pinos", [this]() { pinsMenu(); }           },
#if !defined(LITE_VERSION)
            {"Alternar API BLE", [this]() { enableBLEAPI(); }       },
            {"BadUSB/BLE",      [this]() { setBadUSBBLEMenu(); }   },
#endif
            {"Nome BLE",
             [this]() {
                 String name = keyboard(bruceConfigPins.bleName, 30, "Nome do dispositivo BLE");
                 if (name.length() > 0 && name != "\x1B") bruceConfigPins.setBleName(name);
             }                                                     },
            {"Credenciais de rede", [this]() { setNetworkCredsMenu(); }},
            {"Restaurar fabrica",
             []() {
                 // Confirmation dialog for destructive action
                 drawMainBorder(true);
                 int8_t choice = displayMessage(
                     "Restaurar padrao?\nTodos os dados serao\napagados!",
                     "Nao",
                     nullptr,
                     "Sim",
                     TFT_RED
                 );

                 if (choice == 1) {
                     // User confirmed - perform factory reset
                     bruceConfigPins.factoryReset();
                     bruceConfig.factoryReset(); // Restarts ESP
                 }
                 // If cancelled, loop continues and menu rebuilds
             }                                                     },
            {"Voltar",          []() {}                            },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Avancado");

        // Exit to System Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}
/*********************************************************************
**  Function: powerMenu
**  Power management submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::powerMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Sono profundo", goToDeepSleep          },
            {"Suspender",     setSleepMode           },
            {"Reiniciar",     []() { ESP.restart(); }},
            {"Desligar",
             []() {
                 // Confirmation dialog for power off
                 drawMainBorder(true);
                 int8_t choice = displayMessage("Desligar dispositivo?", "Nao", nullptr, "Sim", TFT_RED);

                 if (choice == 1) { powerOff(); }
             }                                    },
            {"Voltar",     []() {}                },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Energia");

        // Exit to Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: devMenu
**  Developer mode menu for advanced hardware configuration
**********************************************************************/
void ConfigMenu::devMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Serial via USB",   [this]() { switchToUSBSerial(); }          },
            {"Serial via UART",  [this]() { switchToUARTSerial(); }         },
            {"Desativar modo dev", [this]() { bruceConfig.setDevMode(false); }},
            {"Voltar",           []() {}                                    },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Modo dev");

        // Check if "Disable DevMode" was pressed (second-to-last option)
        if (selected == localOptions.size() - 2) {
            returnToMenu = true; // Signal to exit all Config menus
            return;
        }

        // Exit to Config menu on Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: pinsMenu
**  Developer mode menu for advanced hardware configuration
**********************************************************************/
void ConfigMenu::pinsMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Localizar I2C",  [this]() { find_i2c_addresses(); }                      },
            {"Pinos CC1101",   [this]() { setSPIPinsMenu(bruceConfigPins.CC1101_bus); }},
            {"Pinos nRF24",    [this]() { setSPIPinsMenu(bruceConfigPins.NRF24_bus); } },
#if !defined(LITE_VERSION)
            {"Pinos LoRa",     [this]() { setSPIPinsMenu(bruceConfigPins.LoRa_bus); }  },
            {"Pinos ST25R3916", [this]() { setSPIPinsMenu(bruceConfigPins.ST25R_bus); } },
            {"Pinos W5500",    [this]() { setSPIPinsMenu(bruceConfigPins.W5500_bus); } },
#endif
            {"Pinos SD",       [this]() { setSPIPinsMenu(bruceConfigPins.SDCARD_bus); }},
            {"Pinos I2C",      [this]() { setI2CPinsMenu(bruceConfigPins.i2c_bus); }   },
            {"Pinos UART",     [this]() { setUARTPinsMenu(bruceConfigPins.uart_bus); } },
            {"Pinos GPS",      [this]() { setUARTPinsMenu(bruceConfigPins.gps_bus); }  },
            //{"Serial use USB",  [this]() { switchToUSBSerial(); }                       },
            //{"Serial use UART", [this]() { switchToUARTSerial(); }                      },
            {"Voltar",         []() {}                                                 },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_GEAR, "Config. de pinos");

        // Exit to Config menu on Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: switchToUSBSerial
**  Switch serial output to USB Serial
**********************************************************************/
void ConfigMenu::switchToUSBSerial() {
    USBserial.setSerialOutput(&Serial);
    Serial1.end();
}

/*********************************************************************
**  Function: switchToUARTSerial
**  Switch serial output to UART (handles pin conflicts)
**********************************************************************/
void ConfigMenu::switchToUARTSerial() {
    // Check and resolve SD card pin conflicts
    if (bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.uart_bus.tx)) {
        sdcardSPI.end();
    }

    // Check and resolve CC1101/NRF24 pin conflicts
    if (bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.uart_bus.tx) ||
        bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.uart_bus.tx)) {
        AUX_SPI.end();
    }

    // Configure UART pins and switch serial output
    pinMode(bruceConfigPins.uart_bus.rx, INPUT);
    pinMode(bruceConfigPins.uart_bus.tx, OUTPUT);
    Serial1.begin(115200, SERIAL_8N1, bruceConfigPins.uart_bus.rx, bruceConfigPins.uart_bus.tx);
    USBserial.setSerialOutput(&Serial1);
}
/*********************************************************************
**  Function: drawIcon
**  Draw config gear icon
**********************************************************************/
void ConfigMenu::drawIcon(float scale) {
    clearIconArea();
    int radius = scale * 9;

    // Draw 6 gear teeth segments
    for (int i = 0; i < 6; i++) {
        tft.drawArc(
            iconCenterX,
            iconCenterY,
            3.5 * radius,
            2 * radius,
            15 + 60 * i,
            45 + 60 * i,
            bruceConfig.priColor,
            bruceConfig.bgColor,
            true
        );
    }

    // Draw inner circle
    tft.drawArc(
        iconCenterX,
        iconCenterY,
        2.5 * radius,
        radius,
        0,
        360,
        bruceConfig.priColor,
        bruceConfig.bgColor,
        false
    );
}
