#include "MaliQrWebApi.h"

#include "modules/mali/MaliQrService.h"
#include "../lib/TFT_eSPI_QRcode/src/qrcode.h"

#include <cstdlib>

namespace {

bool getPostParam(AsyncWebServerRequest *request, const char *name, String &value)
{
    if (!request->hasParam(name, true)) return false;
    value = request->getParam(name, true)->value();
    return true;
}

bool buildPayload(AsyncWebServerRequest *request, String &payload, String &error)
{
    String type;
    getPostParam(request, "type", type);

    if (type == "pix") {
        String key;
        String amount;
        if (!getPostParam(request, "key", key) || !getPostParam(request, "amount", amount)) {
            error = "PIX requer key e amount";
            return false;
        }
        if (!MaliQrService::buildPixPayload(key, amount, payload, error)) return false;
    } else if (!type.isEmpty()) {
        error = "Tipo de QR Code desconhecido";
        return false;
    } else if (!getPostParam(request, "payload", payload)) {
        error = "Campo payload ausente";
        return false;
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

    switch (MaliQrService::enqueueDisplay(payload)) {
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

void handlePreview(AsyncWebServerRequest *request)
{
    String payload;
    String error;
    if (!buildPayload(request, payload, error)) {
        request->send(400, "text/plain; charset=utf-8", error);
        return;
    }

    if (!MaliQrService::lockEncoder(pdMS_TO_TICKS(25))) {
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
            handleShow(request);
        }
    });

    webServer.on("/api/qr/preview", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate) {
            request->send(500, "text/plain", "Authentication callback unavailable");
        } else if (authenticate(request)) {
            handlePreview(request);
        }
    });
}
