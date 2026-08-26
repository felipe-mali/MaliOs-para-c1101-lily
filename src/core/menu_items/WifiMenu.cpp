#include "WifiMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wg.h"
#include "core/wifi/wifi_common.h"
#include "core/wifi/wifi_mac.h"
#include "modules/ethernet/ARPScanner.h"
#include "modules/mali/MaliWiki.h"
#include "modules/wifi/ap_info.h"
#include "modules/wifi/clients.h"
#include "modules/wifi/evil_portal.h"
#include "modules/wifi/karma_attack.h"
#include "modules/wifi/netcut.h"
#include "modules/wifi/responder.h"
#include "modules/wifi/scan_hosts.h"
#include "modules/wifi/sniffer.h"
#include "modules/wifi/wifi_atks.h"

#ifndef LITE_VERSION
#include "modules/pwnagotchi/pwnagotchi.h"
#include "modules/wifi/channel_analyzer.h"
#include "modules/wifi/jam_detect.h"
#include "modules/wifi/wifi_recover.h"
#endif

// #include "modules/reverseShell/reverseShell.h"
//  Developed by Fourier (github.com/9dl)
//  Use BruceC2 to interact with the reverse shell server
//  BruceC2: https://github.com/9dl/Bruce-C2
//  To use BruceC2:
//  1. Start Reverse Shell Mode in Bruce
//  2. Start BruceC2 and wait.
//  3. Visit 192.168.4.1 in your browser to access the web interface for shell executing.

// 32bit: https://github.com/9dl/Bruce-C2/releases/download/v1.0/BruceC2_windows_386.exe
// 64bit: https://github.com/9dl/Bruce-C2/releases/download/v1.0/BruceC2_windows_amd64.exe
#include "modules/wifi/socks4_proxy.h"
#include "modules/wifi/tcp_utils.h"

// global toggle - controls whether scanNetworks includes hidden SSIDs
bool showHiddenNetworks = false;

void WifiMenu::optionsMenu() {
    returnToMenu = false;
    options.clear();
    // Note: WiFi features will cleanly stop WebUI automatically when they start
    // User can navigate menu normally even with WebUI active
    if (!WiFi.isConnected() && !WiFi.AP.started()) {
        options = {
            {"Conectar ao Wi-Fi", lambdaHelper(wifiConnectMenu, WIFI_STA)},
            {"Iniciar AP Wi-Fi", [=]() {
                 wifiConnectMenu(WIFI_AP);
                 displayInfo("pwd: " + bruceConfig.wifiAp.pwd, true);
             }},
        };
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) { options.push_back({"Desligar Wi-Fi", wifiDisconnect}); }
    if (WiFi.getMode() & WIFI_MODE_STA && WiFi.isConnected()) {
        options.push_back({"Info do AP", displayAPInfo});
    }
    options.push_back({"WebUI", loopOptionsWebUi});
    options.push_back({"Ataques Wi-Fi", wifi_atk_menu});
    options.push_back({"Evil Portal", [=]() {
                           // WebUI cleanup now handled automatically inside EvilPortal constructor
                           EvilPortal();
                       }});
    options.push_back({"NetCut", [=]() { netcutMenu(); }});
    // options.push_back({"ReverseShell", [=]()       { ReverseShell(); }});
#ifndef LITE_VERSION
    options.push_back({"Escutar TCP", listenTcpPort});
    options.push_back({"Cliente TCP", clientTCP});
    options.push_back({"SOCKS4 Proxy", []() { socks4Proxy(1080); }});
    options.push_back({"TelNET", telnet_setup});
    options.push_back({"SSH", lambdaHelper(ssh_setup, String(""))});
    options.push_back({"Sniffer", sniffer_setup});
    options.push_back({"Analisar canal", channel_analyzer_setup});
    options.push_back({"Detectar jammer", jam_detect_setup});
    options.push_back({"Procurar hosts", [=]() {
                           bool doScan = true;
                           if (!WiFi.isConnected()) doScan = wifiConnectMenu();

                           if (doScan) {
                               esp_netif_t *esp_netinterface =
                                   esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                               if (esp_netinterface == nullptr) {
                                   Serial.println("Failed to get netif handle");
                                   return;
                               }
                               ARPScanner{esp_netinterface};
                           }
                       }});
    options.push_back({"Wireguard", wg_setup});
    options.push_back({"Responder", responder});
    options.push_back({"Brucegotchi", brucegotchi_start});
    options.push_back({"Recuperar senha", wifi_recover_menu});
#endif

    options.push_back({"Configurar", [this]() { configMenu(); }});

    addOptionToMainMenu();
    options.push_back({"? Ajuda", []() { MaliWiki::open(MaliWiki::Category::WIFI); }});

    loopOptions(options, MENU_TYPE_SUBMENU, "WiFi");

    options.clear();
}

void WifiMenu::configMenu() {
    std::vector<Option> wifiOptions;

    wifiOptions.push_back({"Alterar MAC", wifiMACMenu});
    wifiOptions.push_back({"Adicionar Evil Wi-Fi", addEvilWifiMenu});
    wifiOptions.push_back({"Remover Evil Wi-Fi", removeEvilWifiMenu});
    wifiOptions.push_back({bruceConfig.TerminalLog ? "Log SSH/Telnet DESL" : "Log SSH/Telnet LIG", [this]() {
                               bruceConfig.setTerminalLog(!bruceConfig.TerminalLog);
                               configMenu();
                           }});

    // Evil Wifi Settings submenu (unchanged)
    wifiOptions.push_back({"Config. Evil Wi-Fi", [this]() {
                               std::vector<Option> evilOptions;

                               evilOptions.push_back({"Definir IP gateway", setEvilGatewayIp});
                               evilOptions.push_back({"Modo de senha", setEvilPasswordMode});
                               evilOptions.push_back({"Renomear /creds", setEvilEndpointCreds});
                               evilOptions.push_back({"Permitir /creds", setEvilAllowGetCreds});
                               evilOptions.push_back({"Renomear /ssid", setEvilEndpointSsid});
                               evilOptions.push_back({"Permitir /ssid", setEvilAllowSetSsid});
                               evilOptions.push_back({"Mostrar endpoints", setEvilAllowEndpointDisplay});
                               evilOptions.push_back({"Voltar", [this]() { configMenu(); }});
                               loopOptions(evilOptions, MENU_TYPE_SUBMENU, "Config. Evil Wi-Fi");
                           }});

    {

        String hidden__wifi_option = String("Redes ocultas: ") + (showHiddenNetworks ? "LIG" : "DESL");

        // construct Option explicitly using char* label
        Option opt(hidden__wifi_option.c_str(), [this]() {
            showHiddenNetworks = !showHiddenNetworks;
            displayInfo(String("Redes ocultas: ") + (showHiddenNetworks ? "LIG" : "DESL"), true);
            configMenu();
        });

        wifiOptions.push_back(opt);
    }
    wifiOptions.push_back({"Voltar", [this]() { optionsMenu(); }});
    loopOptions(wifiOptions, MENU_TYPE_SUBMENU, "Config. Wi-Fi");
}

void WifiMenu::drawIcon(float scale) {
    clearIconArea();
    int deltaY = scale * 20;
    int radius = scale * 6;

    tft.fillCircle(iconCenterX, iconCenterY + deltaY, radius, bruceConfig.priColor);
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        deltaY + radius,
        deltaY,
        130,
        230,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        2 * deltaY + radius,
        2 * deltaY,
        130,
        230,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
}
