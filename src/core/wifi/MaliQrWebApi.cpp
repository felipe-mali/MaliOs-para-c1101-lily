#include "MaliQrWebApi.h"

#include "modules/mali/MaliQrService.h"
#include "modules/mali/MaliQrStore.h"
#include "../lib/TFT_eSPI_QRcode/src/qrcode.h"

#include <ArduinoJson.h>
#include <cstdlib>

namespace {

constexpr size_t kMaxRequestBodyBytes = 1024;

const String *getPostParam(AsyncWebServerRequest *request, const char *name)
{
    if (!request->hasParam(name, true)) return nullptr;
    return &request->getParam(name, true)->value();
}

void sendJson(AsyncWebServerRequest *request, JsonDocument &document, int status = 200)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json; charset=utf-8");
    if (!response) {
        request->send(503, "text/plain; charset=utf-8", "Memoria insuficiente para responder");
        return;
    }
    response->setCode(status);
    response->addHeader("Cache-Control", "no-store");
    serializeJson(document, *response);
    request->send(response);
}

bool parseBoolean(const String *value, bool defaultValue, bool &result)
{
    if (!value) {
        result = defaultValue;
        return true;
    }
    if (*value == "1" || value->equalsIgnoreCase("true")) {
        result = true;
        return true;
    }
    if (*value == "0" || value->equalsIgnoreCase("false")) {
        result = false;
        return true;
    }
    return false;
}

bool buildPayload(AsyncWebServerRequest *request, String &payload, String &error)
{
    const String *typeParam = getPostParam(request, "type");
    if (typeParam && typeParam->length() > 8) {
        error = "Campo type excede o limite";
        return false;
    }
    const String type = typeParam ? *typeParam : String();

    if (type == "pix") {
        const String *key = getPostParam(request, "key");
        const String *amount = getPostParam(request, "amount");
        if (!key || !amount) {
            error = "PIX requer key e amount";
            return false;
        }
        if (key->length() > 25 || amount->length() > 10) {
            error = "Campos PIX excedem o limite";
            return false;
        }
        if (!MaliQrService::buildPixPayload(*key, *amount, payload, error)) return false;
    } else if (!type.isEmpty()) {
        error = "Tipo de QR Code desconhecido";
        return false;
    } else {
        const String *payloadParam = getPostParam(request, "payload");
        if (!payloadParam) {
            error = "Campo payload ausente";
            return false;
        }
        if (payloadParam->length() > MaliQrService::kMaxPayloadBytes) {
            error = "Payload excede o limite de 154 bytes";
            return false;
        }
        payload = *payloadParam;
    }

    return MaliQrService::validatePayload(payload, error);
}

void handleShow(AsyncWebServerRequest *request)
{
    String payload;
    String error;
    if (!buildPayload(request, payload, error)) {
        request->send(400, "text/plain; charset=utf-8", error);
        return;
    }

    bool recordHistory = !payload.startsWith("WIFI:");
    if (!parseBoolean(getPostParam(request, "history"), recordHistory, recordHistory)) {
        request->send(400, "text/plain; charset=utf-8", "Campo history invalido");
        return;
    }

    String historyLabel;
    if (const String *label = getPostParam(request, "label")) {
        if (label->length() > 48) {
            request->send(400, "text/plain; charset=utf-8", "Rotulo excede 48 bytes");
            return;
        }
        historyLabel = *label;
    }

    switch (MaliQrService::enqueueDisplay(payload, recordHistory, historyLabel)) {
        case MaliQrService::QueueResult::Accepted:
            request->send(202, "text/plain; charset=utf-8", "QR Code enfileirado para exibicao");
            return;
        case MaliQrService::QueueResult::Full:
            request->send(409, "text/plain; charset=utf-8", "Ja existe um QR Code aguardando exibicao");
            return;
        case MaliQrService::QueueResult::Invalid:
            request->send(400, "text/plain; charset=utf-8", "Payload de QR Code invalido");
            return;
    }
    request->send(500, "text/plain", "Estado interno invalido");
}

void handleFavoritesList(AsyncWebServerRequest *request)
{
    MaliQrStore::begin();
    JsonDocument document;
    JsonArray items = document["items"].to<JsonArray>();
    const size_t count = MaliQrStore::favoriteCount();
    for (size_t index = 0; index < count; ++index) {
        MaliQrStore::FavoriteEntry entry;
        if (!MaliQrStore::favoriteAt(index, entry)) continue;
        JsonObject item = items.add<JsonObject>();
        item["name"] = entry.name;
        item["payload"] = entry.payload;
    }
    document["max"] = MaliQrStore::kMaxFavorites;
    sendJson(request, document);
}

void handleFavoriteSave(AsyncWebServerRequest *request)
{
    const String *name = getPostParam(request, "name");
    if (!name || name->length() > 48) {
        request->send(400, "text/plain; charset=utf-8", "Nome do favorito ausente ou muito longo");
        return;
    }

    String payload;
    String error;
    if (!buildPayload(request, payload, error)) {
        request->send(400, "text/plain; charset=utf-8", error);
        return;
    }
    if (!MaliQrStore::upsertFavorite(*name, payload, error)) {
        request->send(400, "text/plain; charset=utf-8", error);
        return;
    }

    const String *originalName = getPostParam(request, "originalName");
    if (originalName && *originalName != *name && !originalName->isEmpty()) {
        String removeError;
        if (!MaliQrStore::removeFavorite(*originalName, removeError)) {
            Serial.println("[MaliQrWeb] Favorito renomeado, mas origem nao foi removida: " + removeError);
        }
    }
    request->send(200, "text/plain; charset=utf-8", "Favorito salvo");
}

void handleFavoriteDelete(AsyncWebServerRequest *request)
{
    if (!request->hasParam("name")) {
        request->send(400, "text/plain; charset=utf-8", "Nome do favorito ausente");
        return;
    }
    const String &name = request->getParam("name")->value();
    if (name.length() > 48) {
        request->send(400, "text/plain; charset=utf-8", "Nome do favorito muito longo");
        return;
    }

    String error;
    if (!MaliQrStore::removeFavorite(name, error)) {
        request->send(404, "text/plain; charset=utf-8", error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Favorito excluido");
}

void handleHistoryList(AsyncWebServerRequest *request)
{
    MaliQrStore::begin();
    JsonDocument document;
    document["limit"] = MaliQrStore::historyLimit();
    document["maxLimit"] = MaliQrStore::kMaxHistoryLimit;
    JsonArray items = document["items"].to<JsonArray>();
    const size_t count = MaliQrStore::historyCount();
    for (size_t index = 0; index < count; ++index) {
        MaliQrStore::HistoryEntry entry;
        if (!MaliQrStore::historyAt(index, entry)) continue;
        JsonObject item = items.add<JsonObject>();
        item["label"] = entry.label;
        item["payload"] = entry.payload;
    }
    sendJson(request, document);
}

void handleHistoryClear(AsyncWebServerRequest *request)
{
    if (!MaliQrStore::clearHistory() || !MaliQrStore::flush()) {
        request->send(503, "text/plain; charset=utf-8", "Nao foi possivel limpar o historico");
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Historico limpo");
}

void handleHistoryLimit(AsyncWebServerRequest *request)
{
    const String *value = getPostParam(request, "limit");
    if (!value || value->isEmpty() || value->length() > 2) {
        request->send(400, "text/plain; charset=utf-8", "Limite invalido");
        return;
    }
    char *end = nullptr;
    const long parsed = strtol(value->c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < 1 || parsed > static_cast<long>(MaliQrStore::kMaxHistoryLimit)) {
        request->send(400, "text/plain; charset=utf-8", "Limite deve estar entre 1 e 10");
        return;
    }

    String error;
    if (!MaliQrStore::setHistoryLimit(static_cast<size_t>(parsed), error) ||
        !MaliQrStore::flush()) {
        request->send(503, "text/plain; charset=utf-8", error.isEmpty() ? "Falha ao salvar limite" : error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Limite atualizado");
}

void handlePreview(AsyncWebServerRequest *request)
{
    String payload;
    String error;
    if (!buildPayload(request, payload, error)) {
        request->send(400, "text/plain; charset=utf-8", error);
        return;
    }

    if (!MaliQrService::lockEncoder(0)) {
        request->send(409, "text/plain; charset=utf-8", "Encoder de QR Code ocupado");
        return;
    }

    const size_t packedSize = QRcode::packedSize();
    uint8_t *packed = static_cast<uint8_t *>(malloc(packedSize));
    if (!packed) {
        MaliQrService::unlockEncoder();
        request->send(503, "text/plain; charset=utf-8", "Memoria insuficiente para gerar a previa");
        return;
    }

    uint8_t matrixSize = 0;
    const bool encoded = QRcode::encode(payload, packed, packedSize, matrixSize);
    if (!encoded) {
        free(packed);
        MaliQrService::unlockEncoder();
        request->send(503, "text/plain; charset=utf-8", "Nao foi possivel gerar a previa");
        return;
    }

    AsyncResponseStream *response = request->beginResponseStream("application/octet-stream", packedSize + 1);
    size_t written = 0;
    if (response) {
        written += response->write(matrixSize);
        written += response->write(packed, packedSize);
    }

    free(packed);
    MaliQrService::unlockEncoder();

    if (!response || written != packedSize + 1) {
        delete response;
        request->send(503, "text/plain; charset=utf-8", "Memoria insuficiente para enviar a previa");
        return;
    }

    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-QR-Bit-Order", "row-major-msb-first");
    request->send(response);
}

} // namespace

void registerMaliQrWebApi(AsyncWebServer &webServer, MaliQrWebAuthCallback authenticate)
{
    webServer.on("/api/qr/show", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) {
            request->send(500, "text/plain", "Authentication callback unavailable");
        } else if (authenticate(request)) {
            if (request->contentLength() > kMaxRequestBodyBytes) {
                request->send(413, "text/plain; charset=utf-8", "Corpo da requisicao excede o limite");
            } else {
                handleShow(request);
            }
        }
    });

    webServer.on("/api/qr/preview", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) {
            request->send(500, "text/plain", "Authentication callback unavailable");
        } else if (authenticate(request)) {
            if (request->contentLength() > kMaxRequestBodyBytes) {
                request->send(413, "text/plain; charset=utf-8", "Corpo da requisicao excede o limite");
            } else {
                handlePreview(request);
            }
        }
    });

    webServer.on("/api/qr/favorites", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) handleFavoritesList(request);
    });

    webServer.on("/api/qr/favorites", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) {
            if (request->contentLength() > kMaxRequestBodyBytes) {
                request->send(413, "text/plain; charset=utf-8", "Corpo da requisicao excede o limite");
            } else {
                handleFavoriteSave(request);
            }
        }
    });

    webServer.on("/api/qr/favorites", HTTP_DELETE, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) handleFavoriteDelete(request);
    });

    webServer.on("/api/qr/history", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) handleHistoryList(request);
    });

    webServer.on("/api/qr/history/clear", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) handleHistoryClear(request);
    });

    webServer.on("/api/qr/history/limit", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) request->send(500, "text/plain", "Authentication callback unavailable");
        else if (authenticate(request)) {
            if (request->contentLength() > kMaxRequestBodyBytes) {
                request->send(413, "text/plain; charset=utf-8", "Corpo da requisicao excede o limite");
            } else {
                handleHistoryLimit(request);
            }
        }
    });
}
