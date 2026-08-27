#include "MaliWifiWebApi.h"

#include "core/wifi/wifi_common.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstring>
#include <freertos/queue.h>
#include <globals.h>

namespace {

enum class WifiActionType : uint8_t {
    Connect,
    DisconnectSta,
    StartAp,
    StopAp,
};

struct PendingWifiAction {
    WifiActionType type;
    char ssid[33];
    char password[65];
};

StaticQueue_t actionQueueControl;
uint8_t actionQueueStorage[sizeof(PendingWifiAction)];
QueueHandle_t actionQueue = nullptr;
portMUX_TYPE actionInitMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool actionBusy = false;
volatile bool connectionPending = false;
uint32_t connectionStartedAt = 0;
char lastAction[48] = "Pronto";
char lastError[80] = "";
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;

constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr size_t kMaxRequestBodyBytes = 512;

void updateStatus(const char *action, const char *error = "") {
    portENTER_CRITICAL(&statusMux);
    strlcpy(lastAction, action ? action : "", sizeof(lastAction));
    strlcpy(lastError, error ? error : "", sizeof(lastError));
    portEXIT_CRITICAL(&statusMux);
}

const String *postParam(AsyncWebServerRequest *request, const char *name) {
    if (!request->hasParam(name, true)) return nullptr;
    return &request->getParam(name, true)->value();
}

bool hasEmbeddedNul(const String &value) {
    return strlen(value.c_str()) != value.length();
}

bool isHexPsk(const String &value) {
    if (value.length() != 64) return false;
    for (size_t index = 0; index < value.length(); ++index) {
        const char c = value.charAt(index);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

const char *modeName(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_STA: return "STA";
        case WIFI_MODE_AP: return "AP";
        case WIFI_MODE_APSTA: return "AP+STA";
        default: return "OFF";
    }
}

bool enqueue(const PendingWifiAction &action) {
    MaliWifiWebApi::begin();
    if (!actionQueue || actionBusy || connectionPending) return false;
    if (xQueueSend(actionQueue, &action, 0) != pdPASS) return false;
    actionBusy = true;
    return true;
}

void sendStatus(AsyncWebServerRequest *request) {
    const wifi_mode_t mode = WiFi.getMode();
    const bool staConnected = WiFi.status() == WL_CONNECTED;
    const bool apActive = (mode & WIFI_MODE_AP) != 0;

    JsonDocument document;
    char actionSnapshot[sizeof(lastAction)];
    char errorSnapshot[sizeof(lastError)];
    portENTER_CRITICAL(&statusMux);
    memcpy(actionSnapshot, lastAction, sizeof(actionSnapshot));
    memcpy(errorSnapshot, lastError, sizeof(errorSnapshot));
    portEXIT_CRITICAL(&statusMux);
    actionSnapshot[sizeof(actionSnapshot) - 1] = '\0';
    errorSnapshot[sizeof(errorSnapshot) - 1] = '\0';
    document["connected"] = staConnected;
    document["mode"] = modeName(mode);
    document["busy"] = actionBusy || connectionPending;
    document["lastAction"] = actionSnapshot;
    document["lastError"] = errorSnapshot;
    document["mac"] = WiFi.macAddress();
    if (staConnected) {
        document["ssid"] = WiFi.SSID();
        document["ip"] = WiFi.localIP().toString();
        document["rssi"] = WiFi.RSSI();
        document["channel"] = WiFi.channel();
    } else {
        document["ssid"] = "";
        document["ip"] = "";
        document["rssi"] = nullptr;
        document["channel"] = nullptr;
    }
    document["ap"]["active"] = apActive;
    document["ap"]["ssid"] = apActive ? WiFi.softAPSSID() : String();
    document["ap"]["ip"] = apActive ? WiFi.softAPIP().toString() : String();
    document["ap"]["stations"] = apActive ? WiFi.softAPgetStationNum() : 0;

    AsyncResponseStream *response = request->beginResponseStream("application/json; charset=utf-8");
    if (!response) {
        request->send(503, "text/plain; charset=utf-8", "Memoria insuficiente");
        return;
    }
    response->addHeader("Cache-Control", "no-store");
    serializeJson(document, *response);
    request->send(response);
}

void handleConnect(AsyncWebServerRequest *request) {
    if (request->contentLength() > kMaxRequestBodyBytes) {
        request->send(413, "text/plain; charset=utf-8", "Corpo da requisicao excede o limite");
        return;
    }
    const String *ssid = postParam(request, "ssid");
    const String *password = postParam(request, "password");
    const String *security = postParam(request, "security");
    if (!ssid || ssid->isEmpty() || ssid->length() > 32 || hasEmbeddedNul(*ssid)) {
        request->send(400, "text/plain; charset=utf-8", "SSID ausente ou invalido");
        return;
    }
    if (!password || password->length() > 64 || hasEmbeddedNul(*password)) {
        request->send(400, "text/plain; charset=utf-8", "Senha invalida");
        return;
    }
    const bool openNetwork = security && *security == "open";
    if (!openNetwork && !((password->length() >= 8 && password->length() <= 63) || isHexPsk(*password))) {
        request->send(400, "text/plain; charset=utf-8", "Senha WPA deve ter 8 a 63 bytes ou 64 hex");
        return;
    }
    if (openNetwork && !password->isEmpty()) {
        request->send(400, "text/plain; charset=utf-8", "Rede aberta nao deve enviar senha");
        return;
    }

    PendingWifiAction action = {};
    action.type = WifiActionType::Connect;
    memcpy(action.ssid, ssid->c_str(), ssid->length());
    memcpy(action.password, password->c_str(), password->length());
    if (!enqueue(action)) {
        request->send(409, "text/plain; charset=utf-8", "Ja existe uma acao Wi-Fi em andamento");
        return;
    }
    request->send(202, "text/plain; charset=utf-8", "Conexao Wi-Fi enfileirada");
}

void handleSimpleAction(AsyncWebServerRequest *request, WifiActionType type, const char *message) {
    PendingWifiAction action = {};
    action.type = type;
    if (!enqueue(action)) {
        request->send(409, "text/plain; charset=utf-8", "Ja existe uma acao Wi-Fi em andamento");
        return;
    }
    request->send(202, "text/plain; charset=utf-8", message);
}

} // namespace

namespace MaliWifiWebApi {

void begin() {
    if (actionQueue) return;
    portENTER_CRITICAL(&actionInitMux);
    if (!actionQueue) {
        actionQueue = xQueueCreateStatic(
            1, sizeof(PendingWifiAction), actionQueueStorage, &actionQueueControl
        );
    }
    portEXIT_CRITICAL(&actionInitMux);
}

void service() {
    begin();

    if (connectionPending) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            wifiIP = WiFi.localIP().toString();
            connectionPending = false;
            actionBusy = false;
            updateStatus("Conectado ao Wi-Fi");
            Serial.println("[MaliWifiWeb] Conexao STA concluida");
        } else if (static_cast<uint32_t>(millis() - connectionStartedAt) >= kConnectTimeoutMs) {
            const bool keepAp = (WiFi.getMode() & WIFI_MODE_AP) != 0;
            WiFi.disconnect(false, true);
            vTaskDelay(pdMS_TO_TICKS(20));
            WiFi.mode(keepAp ? WIFI_MODE_AP : WIFI_MODE_NULL);
            wifiConnected = keepAp;
            wifiIP = keepAp ? WiFi.softAPIP().toString() : String();
            connectionPending = false;
            actionBusy = false;
            updateStatus("Falha ao conectar", "Tempo limite de conexao");
            Serial.println("[MaliWifiWeb] Tempo limite ao conectar; dispositivo mantido ativo");
        }
        return;
    }

    PendingWifiAction action = {};
    if (!actionQueue || xQueueReceive(actionQueue, &action, 0) != pdPASS) return;
    updateStatus("Processando acao Wi-Fi");

    switch (action.type) {
        case WifiActionType::Connect: {
            ensureWifiPlatform();
            const bool keepAp = (WiFi.getMode() & WIFI_MODE_AP) != 0;
            WiFi.mode(keepAp ? WIFI_MODE_APSTA : WIFI_MODE_STA);
            vTaskDelay(pdMS_TO_TICKS(20));
            WiFi.begin(action.ssid, action.password);
            memset(action.password, 0, sizeof(action.password));
            connectionStartedAt = millis();
            connectionPending = true;
            updateStatus("Conectando ao Wi-Fi");
            Serial.println("[MaliWifiWeb] Conexao STA iniciada");
            break;
        }
        case WifiActionType::DisconnectSta: {
            const bool keepAp = (WiFi.getMode() & WIFI_MODE_AP) != 0;
            WiFi.disconnect(false, true);
            vTaskDelay(pdMS_TO_TICKS(20));
            WiFi.mode(keepAp ? WIFI_MODE_AP : WIFI_MODE_NULL);
            wifiConnected = keepAp;
            wifiIP = keepAp ? WiFi.softAPIP().toString() : String();
            actionBusy = false;
            updateStatus("STA desconectado");
            Serial.println("[MaliWifiWeb] STA desconectado");
            break;
        }
        case WifiActionType::StartAp: {
            const bool keepSta = WiFi.status() == WL_CONNECTED;
            WiFi.mode(keepSta ? WIFI_MODE_APSTA : WIFI_MODE_AP);
            vTaskDelay(pdMS_TO_TICKS(20));
            if (_setupAP()) {
                updateStatus("AP do MaliOS iniciado");
            } else {
                updateStatus("Falha ao iniciar AP", "Wi-Fi recusou a configuracao do AP");
            }
            actionBusy = false;
            break;
        }
        case WifiActionType::StopAp: {
            WiFi.softAPdisconnect(false);
            vTaskDelay(pdMS_TO_TICKS(20));
            const bool keepSta = WiFi.status() == WL_CONNECTED;
            WiFi.mode(keepSta ? WIFI_MODE_STA : WIFI_MODE_NULL);
            wifiConnected = keepSta;
            wifiIP = keepSta ? WiFi.localIP().toString() : String();
            actionBusy = false;
            updateStatus("AP do MaliOS parado");
            Serial.println("[MaliWifiWeb] AP parado");
            break;
        }
    }
}

void registerRoutes(AsyncWebServer &server, MaliWifiWebAuthCallback authenticate) {
    server.on("/api/wifi/status", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) sendStatus(request);
    });

    server.on("/api/wifi/connect", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) handleConnect(request);
    });

    server.on("/api/wifi/disconnect", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) {
            handleSimpleAction(request, WifiActionType::DisconnectSta, "Desconexao enfileirada");
        }
    });

    server.on("/api/wifi/ap/start", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) {
            handleSimpleAction(request, WifiActionType::StartAp, "Inicio do AP enfileirado");
        }
    });

    server.on("/api/wifi/ap/stop", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) {
            handleSimpleAction(request, WifiActionType::StopAp, "Parada do AP enfileirada");
        }
    });
}

} // namespace MaliWifiWebApi
