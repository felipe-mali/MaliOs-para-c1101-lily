#include "core/ui/MaliUI.h"
#include "core/ui/PtBr.h"
bool isConnectedWireguard = false;
#ifndef LITE_VERSION
#include "wg.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/wifi/wifi_common.h"
#include <WireGuard-ESP32.h>
#include <globals.h>

char private_key[45];
IPAddress local_ip;
char public_key[45];
char endpoint_address[16];
int endpoint_port = 31337;

static constexpr const uint32_t UPDATE_INTERVAL_MS = 5000;

static WireGuard wg;

/*********************************************************************
**  Function: parse_config_file
**  parses wireguard config file wg.conf
**********************************************************************/
void parse_config_file(File configFile) {
    String line;

    while (configFile.available()) {
        line = configFile.readStringUntil('\n');
        Serial.println("==========PRINTING LINE");
        Serial.println(line);
        line.trim();

        if (line.startsWith("[Interface]") || line.isEmpty()) {
            // Skip [Interface] or empty lines
            continue;
        } else if (line.startsWith("PrivateKey")) {
            line.remove(0, line.indexOf('=') + 1);
            line.trim();
            Serial.println("Private Key: " + line);
            strncpy(private_key, line.c_str(), sizeof(private_key) - 1);
            private_key[sizeof(private_key) - 1] = '\0'; // Ensure null-terminated
        } else if (line.startsWith("Address")) {
            line.remove(0, line.indexOf('=') + 1);
            line.trim();
            Serial.println("Local IP: " + line);
            int slashIndex = line.indexOf('/');

            if (slashIndex != -1) {
                Serial.println("~~~~~~~~~~~~");
                Serial.println(line.substring(0, slashIndex));
                local_ip.fromString(line.substring(0, slashIndex));
            }

        } else if (line.startsWith("[Peer]")) {
            // add [Peer] section
        } else if (line.startsWith("PublicKey")) {
            line.remove(0, line.indexOf('=') + 1);
            line.trim();
            Serial.println("Public Key: " + line);
            strncpy(public_key, line.c_str(), sizeof(public_key) - 1);
            public_key[sizeof(public_key) - 1] = '\0'; // Ensure null-terminated
        } else if (line.startsWith("Endpoint")) {
            line.remove(0, line.indexOf('=') + 1);
            line.trim();
            int colonIndex = line.indexOf(':');

            if (colonIndex != -1) {
                // Serial.println("Endpoint Line: " + line);
                strncpy(
                    endpoint_address, line.substring(0, colonIndex).c_str(), sizeof(endpoint_address) - 1
                );
                endpoint_address[sizeof(endpoint_address) - 1] = '\0'; // Ensure null-terminated
                Serial.println("Endpoint Address: " + String(endpoint_address));
                endpoint_port = line.substring(colonIndex + 1).toInt();
                Serial.println("Endpoint Port: " + String(endpoint_port));
            }
        }
    }

    Serial.println("Closing file!");
    configFile.close();
}

/*********************************************************************
**  Function: read_and_parse_file
**  tries to open file wg.conf on local SD
**********************************************************************/
void read_and_parse_file() {
    if (!setupSdCard()) {
        sdcardMounted = false;
        Serial.println("Failed to initialize SD card");
        return;
    }

    File file = SD.open("/wg.conf");
    if (!file) {
        Serial.println("Failed to open wg.conf file");
        displayError("Arquivo wg.conf nao encontrado no SD", true);
        return;
    }

    Serial.println("Readed config file!");

    Serial.println("Found file!");
    parse_config_file(file);
}

/*********************************************************************
**  Function: wg_setup
**  connect to wireguard tunnel
**********************************************************************/
void wg_setup() {
    if (!wifiConnected) wifiConnectMenu();

    read_and_parse_file();

    drawMainBorderWithTitle("WIREGUARD");
    printSubtitle("Conectando...");
    tft.setTextSize(FP);
    padprintln("");
    padprintln("Sincronizando horario...");

    configTime(9 * 60 * 60, 0, "ntp.jst.mfeed.ad.jp", "ntp.nict.jp");

    padprintln("Iniciando tunel...");
    wg.begin(local_ip, private_key, endpoint_address, public_key, endpoint_port);

    drawMainBorderWithTitle("WIREGUARD");
    printSubtitle("Conectado");
    tft.setTextSize(FP);
    padprintln("");
    tft.setTextColor(MaliUI::SUCCESS, bruceConfig.bgColor);
    padprintln("Status: Conectado");
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    padprintln("");
    padprintln("IP do tunel:");
    tft.setTextColor(MaliUI::TEXT_PRIMARY, bruceConfig.bgColor);
    padprintln(local_ip.toString());
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    padprintln("");
    padprintln("Destino: " + String(endpoint_address));
    padprintln("Porta: " + String(endpoint_port));
    padprintln("");
    printFootnote(MaliText::press_any_key_to_return_f3437b);

    isConnectedWireguard = true;

    while (!check(AnyKeyPress)) { delay(100); }
}
#endif
