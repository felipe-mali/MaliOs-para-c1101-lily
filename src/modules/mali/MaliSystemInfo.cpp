#include "MaliSystemInfo.h"

#include "core/scrollableTextArea.h"
#include "core/utils.h"
#include <globals.h>

#include <WiFi.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <soc/soc_caps.h>

#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "N/D"
#endif

namespace {
String formatUptime() {
    uint64_t totalSeconds = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
    uint32_t days = totalSeconds / 86400ULL;
    uint8_t hours = (totalSeconds % 86400ULL) / 3600ULL;
    uint8_t minutes = (totalSeconds % 3600ULL) / 60ULL;
    uint8_t seconds = totalSeconds % 60ULL;

    String result;
    if (days > 0) result += String(days) + "d ";
    result += String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
    return result;
}

String deviceModel() {
#if defined(T_EMBED_1101)
    return "LILYGO T-Embed CC1101 Plus";
#elif defined(ARDUINO_BOARD)
    return String(ARDUINO_BOARD);
#elif defined(DEVICE_NAME)
    return String(DEVICE_NAME);
#else
    return "ESP32";
#endif
}

String stationMac() {
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return "N/D";

    char text[18];
    snprintf(
        text,
        sizeof(text),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
    return String(text);
}

bool spiPinsReady(const BruceConfigPins::SPIPins &bus) {
    return bus.sck != GPIO_NUM_NC && bus.miso != GPIO_NUM_NC && bus.mosi != GPIO_NUM_NC &&
           bus.cs != GPIO_NUM_NC && bus.io0 != GPIO_NUM_NC;
}

bool uartPinsReady(const BruceConfigPins::UARTPins &bus) {
    return bus.rx != GPIO_NUM_NC && bus.tx != GPIO_NUM_NC;
}

void showArea(ScrollableTextArea &area) {
    area.draw(true);
    delay(120);
    check(EscPress);
    check(SelPress);

    while (true) {
#ifdef HAS_ENCODER
        int32_t steps = drainRotarySteps();
        while (steps > 0) {
            area.scrollUp();
            --steps;
        }
        while (steps < 0) {
            area.scrollDown();
            ++steps;
        }
#endif
        if (check(PrevPress) || check(UpPress)) area.scrollUp();
        if (check(NextPress) || check(DownPress)) area.scrollDown();
        if (check(EscPress) || check(SelPress)) break;

        area.draw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

const char *configuredState(bool supported, bool configured) {
    if (!supported) return "Indisponivel";
    return configured ? "Disponivel" : "Nao detectado";
}
} // namespace

namespace MaliSystemInfo {
void showSystemInfo() {
    ScrollableTextArea area("INFO. DO SISTEMA");

    area.addLine("MaliOS: " + String(MALIOS_VERSION));
    area.addLine("Bruce base: " + String(BRUCE_VERSION));
    area.addLine("Chip: " + String(ESP.getChipModel()));
    area.addLine("Revisao: " + String(ESP.getChipRevision()));
    area.addLine("CPU: " + String(ESP.getCpuFreqMHz()) + " MHz");
    area.addLine("Flash total: " + formatBytes(ESP.getFlashChipSize()));
    area.addLine("Heap livre: " + formatBytes(ESP.getFreeHeap()));
    area.addLine("Heap total: " + formatBytes(ESP.getHeapSize()));
    area.addLine("PSRAM total: " + formatBytes(ESP.getPsramSize()));
    area.addLine("PSRAM livre: " + formatBytes(ESP.getFreePsram()));
    area.addLine("Tempo ligado: " + formatUptime());
    area.addLine("MAC: " + stationMac());
    area.addLine("Build: " + String(GIT_COMMIT_HASH));
    area.addLine("Compilado: " + String(__DATE__) + " " + String(__TIME__));

    showArea(area);
}

void showHardwareStatus() {
    ScrollableTextArea area("STATUS DO HARDWARE");
    area.addLine("Leitura local; sem transmitir");
    area.addLine("");

#if SOC_WIFI_SUPPORTED
    area.addLine(String("Wi-Fi: ") + (WiFi.getMode() == WIFI_MODE_NULL ? "Desativado" : "Ativado"));
#else
    area.addLine("Wi-Fi: Indisponivel");
#endif

#if SOC_BLE_SUPPORTED
    area.addLine(String("Bluetooth/BLE: ") + (NimBLEDevice::isInitialized() ? "Ativado" : "Desativado"));
#else
    area.addLine("Bluetooth/BLE: Indisponivel");
#endif

#if defined(USE_CC1101_VIA_SPI)
    area.addLine(
        String("CC1101: ") +
        configuredState(true, bruceConfigPins.rfModule == CC1101_SPI_MODULE && spiPinsReady(bruceConfigPins.CC1101_bus))
    );
#else
    area.addLine("CC1101: Indisponivel");
#endif

#if defined(USE_NRF24_VIA_SPI)
    area.addLine(String("nRF24: ") + configuredState(true, spiPinsReady(bruceConfigPins.NRF24_bus)));
#else
    area.addLine("nRF24: Indisponivel");
#endif

    bool pn532Configured =
        bruceConfigPins.rfidModule == PN532_I2C_MODULE || bruceConfigPins.rfidModule == PN532_SPI_MODULE ||
        bruceConfigPins.rfidModule == PN532_I2C_SPI_MODULE;
    area.addLine(String("PN532: ") + configuredState(pn532Configured, pn532Configured));

    bool sdConfigured = bruceConfigPins.SDCARD_bus.cs != GPIO_NUM_NC;
    area.addLine(
        String("SD: ") + (sdcardMounted ? "Ativado" : (sdConfigured ? "Disponivel" : "Indisponivel"))
    );

    area.addLine(
        String("GPS: ") +
        (gpsConnected ? "Ativado" : (uartPinsReady(bruceConfigPins.gps_bus) ? "Disponivel" : "Indisponivel"))
    );

    showArea(area);
}

void showEnergy() {
    ScrollableTextArea area("ENERGIA");
    int battery = getBattery();
    area.addLine("Bateria: " + String(battery > 0 ? String(battery) + "%" : String("N/D")));

#ifdef USE_BQ27220_VIA_I2C
    area.addLine("Tensao: " + String(static_cast<double>(bq.getVolt(VOLT_MODE::VOLT)) / 1000.0, 2) + " V");
    area.addLine(String("Carregando: ") + (isCharging() ? "Sim" : "Nao"));
#else
    area.addLine("Tensao: N/D");
#if defined(ANALOG_BAT_PIN)
    area.addLine(String("Carregando: ") + (isCharging() ? "Sim" : "Nao"));
#else
    area.addLine("Carregando: N/D");
#endif
#endif

    area.addLine("Tempo ligado: " + formatUptime());
    showArea(area);
}

void showAbout() {
    ScrollableTextArea area("SOBRE O MALIOS");
    area.addLine("MaliOS");
    area.addLine("System Core");
    area.addLine("");
    area.addLine("Baseado no projeto Bruce.");
    area.addLine("Creditos e licenca preservados.");
    area.addLine("");
    area.addLine("MaliOS: " + String(MALIOS_VERSION));
    area.addLine("Bruce base: " + String(BRUCE_VERSION));
    area.addLine("Build: " + String(GIT_COMMIT_HASH));
    area.addLine("Dispositivo: " + deviceModel());
    showArea(area);
}
} // namespace MaliSystemInfo
