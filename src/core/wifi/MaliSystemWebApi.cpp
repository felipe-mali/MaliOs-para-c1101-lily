#include "MaliSystemWebApi.h"

#include "core/utils.h"
#include "modules/mali/MaliSystemInfo.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <globals.h>
#include <soc/soc_caps.h>

#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif

namespace {

const char *resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN: return "Desconhecido";
        case ESP_RST_POWERON: return "Power-on";
        case ESP_RST_EXT: return "Reset externo";
        case ESP_RST_SW: return "Software";
        case ESP_RST_PANIC: return "Panic";
        case ESP_RST_INT_WDT: return "Watchdog de interrupcao";
        case ESP_RST_TASK_WDT: return "Watchdog de tarefa";
        case ESP_RST_WDT: return "Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO";
        case ESP_RST_USB: return "USB";
        case ESP_RST_JTAG: return "JTAG";
        case ESP_RST_EFUSE: return "eFuse";
        case ESP_RST_PWR_GLITCH: return "Falha de alimentacao";
        case ESP_RST_CPU_LOCKUP: return "CPU lockup";
    }
    return "Nao mapeado";
}

const char *wifiModeName(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_NULL: return "Desativado";
        case WIFI_MODE_STA: return "STA";
        case WIFI_MODE_AP: return "AP";
        case WIFI_MODE_APSTA: return "AP + STA";
        default: return "Desconhecido";
    }
}

String deviceModel() {
#if defined(T_EMBED_1101)
    return "LILYGO T-Embed CC1101 Plus";
#elif defined(ARDUINO_BOARD)
    return String(ARDUINO_BOARD);
#elif defined(DEVICE_NAME)
    return String(DEVICE_NAME);
#else
    return String(ESP.getChipModel());
#endif
}

void addHardware(JsonArray &items, const char *name, const char *status, const char *detail) {
    JsonObject item = items.add<JsonObject>();
    item["name"] = name;
    item["status"] = status;
    item["detail"] = detail;
}

void handleStatus(AsyncWebServerRequest *request) {
    JsonDocument document;
    document["model"] = deviceModel();
    document["firmware"] = "MaliOS";
    document["maliVersion"] = MALIOS_VERSION;
    document["bruceVersion"] = BRUCE_VERSION;
    document["chip"] = ESP.getChipModel();
    document["chipRevision"] = ESP.getChipRevision();
    document["cpuMHz"] = ESP.getCpuFreqMHz();
    document["uptimeSeconds"] = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;

    const esp_reset_reason_t resetReason = esp_reset_reason();
    JsonObject reset = document["reset"].to<JsonObject>();
    reset["code"] = static_cast<int>(resetReason);
    reset["reason"] = resetReasonName(resetReason);

    JsonObject memory = document["memory"].to<JsonObject>();
    memory["heapFree"] = ESP.getFreeHeap();
    memory["heapTotal"] = ESP.getHeapSize();
    memory["heapMinimum"] = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    memory["heapLargest"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    memory["psramPresent"] = psramFound();
    memory["psramFree"] = ESP.getFreePsram();
    memory["psramTotal"] = ESP.getPsramSize();

    JsonObject storage = document["storage"].to<JsonObject>();
    storage["flashTotal"] = ESP.getFlashChipSize();
    JsonObject littlefs = storage["littlefs"].to<JsonObject>();
    littlefs["total"] = LittleFS.totalBytes();
    littlefs["used"] = LittleFS.usedBytes();
    JsonObject sd = storage["sd"].to<JsonObject>();
    sd["mounted"] = sdcardMounted;
    if (sdcardMounted) {
        sd["total"] = SD.totalBytes();
        sd["used"] = SD.usedBytes();
    } else {
        sd["total"] = nullptr;
        sd["used"] = nullptr;
    }

    JsonObject network = document["network"].to<JsonObject>();
    network["mode"] = wifiModeName(WiFi.getMode());
    network["connected"] = WiFi.isConnected();
    network["ssid"] = WiFi.isConnected() ? WiFi.SSID() : String();
    network["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : String();
    if (WiFi.isConnected()) network["rssi"] = WiFi.RSSI();
    else network["rssi"] = nullptr;
    network["apActive"] = WiFi.AP.started();
    network["apIp"] = WiFi.AP.started() ? WiFi.softAPIP().toString() : String();

    const int battery = getBattery();
    JsonObject energy = document["energy"].to<JsonObject>();
    if (battery >= 0 && battery <= 100) energy["batteryPercent"] = battery;
    else energy["batteryPercent"] = nullptr;
    energy["charging"] = isCharging();

    JsonArray hardware = document["hardware"].to<JsonArray>();
    addHardware(
        hardware,
        "Wi-Fi",
        WiFi.getMode() == WIFI_MODE_NULL ? "Inativo" : "Ativo",
        "Estado verificado pelo driver Wi-Fi"
    );
#if SOC_BLE_SUPPORTED
    addHardware(
        hardware,
        "BLE",
        NimBLEDevice::isInitialized() ? "Ativo" : "Nao verificado",
        NimBLEDevice::isInitialized() ? "Pilha NimBLE inicializada" : "Presenca nao testada pelo Dashboard"
    );
#else
    addHardware(hardware, "BLE", "Indisponivel", "Sem suporte nesta build");
#endif
#if defined(USE_CC1101_VIA_SPI)
    addHardware(hardware, "CC1101", "Nao verificado", "Suporte compilado; o Dashboard nao reinicializa o radio");
#else
    addHardware(hardware, "CC1101", "Indisponivel", "Sem suporte nesta build");
#endif
#if defined(USE_NRF24_VIA_SPI)
    addHardware(hardware, "NRF24", "Nao verificado", "Suporte compilado; modulo externo nao sondado");
#else
    addHardware(hardware, "NRF24", "Nao verificado", "Modulo externo nao sondado");
#endif
    addHardware(hardware, "NFC / RFID", "Nao verificado", "O Dashboard nao ocupa I2C, SPI ou UART para sondagem");
    addHardware(hardware, "IR", "Nao verificado", "Pinos e emissor nao sao sondados nesta pagina");
    addHardware(hardware, "FM / SI4713", "Nao verificado", "Modulo externo nao sondado");
    addHardware(
        hardware,
        "microSD",
        sdcardMounted ? "Ativo" : "Nao verificado",
        sdcardMounted ? "Sistema de arquivos montado" : "Cartao nao montado; ausencia nao confirmada"
    );
    addHardware(
        hardware,
        "PSRAM",
        psramFound() ? "Ativo" : "Nao verificado",
        psramFound() ? "Detectada pelo ESP32" : "Nao detectada nesta execucao"
    );
    addHardware(hardware, "USB", "Nao verificado", "Modo atual da interface USB nao e alterado pelo Dashboard");

    AsyncResponseStream *response = request->beginResponseStream("application/json; charset=utf-8");
    if (!response) {
        request->send(503, "text/plain; charset=utf-8", "Memoria insuficiente para o Dashboard");
        return;
    }
    response->addHeader("Cache-Control", "no-store");
    serializeJson(document, *response);
    request->send(response);
}

} // namespace

void registerMaliSystemWebApi(AsyncWebServer &webServer, MaliSystemWebAuthCallback authenticate) {
    webServer.on("/api/system/status", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleStatus(request);
    });
}
