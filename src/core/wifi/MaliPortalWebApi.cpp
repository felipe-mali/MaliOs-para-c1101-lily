#include "MaliPortalWebApi.h"

#include "modules/mali/MaliPortalStore.h"

#include <ArduinoJson.h>

namespace {

constexpr size_t kMaxEncodedRequestBytes = 32 * 1024;

const String *postParam(AsyncWebServerRequest *request, const char *name) {
    if (!request->hasParam(name, true)) return nullptr;
    return &request->getParam(name, true)->value();
}

void sendError(AsyncWebServerRequest *request, int status, const String &message) {
    request->send(status, "text/plain; charset=utf-8", message);
}

void handleList(AsyncWebServerRequest *request) {
    std::vector<MaliPortalStore::TemplateInfo> templates;
    String error;
    if (!MaliPortalStore::list(templates, error)) {
        sendError(request, 503, error);
        return;
    }
    JsonDocument document;
    JsonArray items = document["items"].to<JsonArray>();
    for (const auto &entry : templates) {
        JsonObject item = items.add<JsonObject>();
        item["name"] = entry.name;
        item["size"] = entry.size;
        item["selected"] = entry.selected;
        item["builtIn"] = entry.builtIn;
    }
    document["maxTemplates"] = MaliPortalStore::kMaxTemplates;
    document["maxTemplateBytes"] = MaliPortalStore::kMaxTemplateBytes;
    AsyncResponseStream *response = request->beginResponseStream("application/json; charset=utf-8");
    if (!response) {
        sendError(request, 503, "Memoria insuficiente para listar templates");
        return;
    }
    response->addHeader("Cache-Control", "no-store");
    serializeJson(document, *response);
    request->send(response);
}

void handleRead(AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
        sendError(request, 400, "Nome do template ausente");
        return;
    }
    String content;
    String error;
    if (!MaliPortalStore::read(request->getParam("name")->value(), content, error)) {
        sendError(request, 404, error);
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain; charset=utf-8", content);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Content-Type-Options", "nosniff");
    request->send(response);
}

void handleSave(AsyncWebServerRequest *request) {
    if (request->contentLength() > kMaxEncodedRequestBytes) {
        sendError(request, 413, "Requisicao excede 32 KiB");
        return;
    }
    const String *name = postParam(request, "name");
    const String *content = postParam(request, "content");
    if (!name || !content) {
        sendError(request, 400, "Nome e conteudo sao obrigatorios");
        return;
    }
    String error;
    if (!MaliPortalStore::save(*name, *content, error)) {
        sendError(request, 400, error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Template salvo");
}

void handleDuplicate(AsyncWebServerRequest *request) {
    const String *source = postParam(request, "source");
    const String *destination = postParam(request, "destination");
    if (!source || !destination) {
        sendError(request, 400, "Origem e destino sao obrigatorios");
        return;
    }
    String error;
    if (!MaliPortalStore::duplicate(*source, *destination, error)) {
        sendError(request, 400, error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Template duplicado");
}

void handleSelect(AsyncWebServerRequest *request) {
    const String *name = postParam(request, "name");
    if (!name) {
        sendError(request, 400, "Nome do template ausente");
        return;
    }
    String error;
    if (!MaliPortalStore::select(*name, error)) {
        sendError(request, 400, error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Template selecionado");
}

void handleDelete(AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
        sendError(request, 400, "Nome do template ausente");
        return;
    }
    String error;
    if (!MaliPortalStore::remove(request->getParam("name")->value(), error)) {
        sendError(request, 400, error);
        return;
    }
    request->send(200, "text/plain; charset=utf-8", "Template excluido");
}

} // namespace

void registerMaliPortalWebApi(AsyncWebServer &webServer, MaliPortalWebAuthCallback authenticate) {
    String beginError;
    if (!MaliPortalStore::begin(beginError)) Serial.println("[MaliPortal] " + beginError);

    webServer.on("/api/portal/templates", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleList(request);
    });
    webServer.on("/api/portal/template", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleRead(request);
    });
    webServer.on("/api/portal/template", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleSave(request);
    });
    webServer.on("/api/portal/template", HTTP_DELETE, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleDelete(request);
    });
    webServer.on("/api/portal/duplicate", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleDuplicate(request);
    });
    webServer.on("/api/portal/select", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (authenticate(request)) handleSelect(request);
    });
}
