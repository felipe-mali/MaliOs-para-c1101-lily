#include "KeyGaugeWebApi.h"
#include "mali_tools/key_gauge/KeyGaugeStore.h"
#include <ArduinoJson.h>
namespace {
void reply(AsyncWebServerRequest *request, JsonDocument &doc, int status = 200) {
    auto *response = request->beginResponseStream("application/json; charset=utf-8");
    if (!response) { request->send(503, "text/plain", "Memoria insuficiente"); return; }
    response->setCode(status);
    response->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *response); request->send(response);
}
void error(AsyncWebServerRequest *request, int status) {
    JsonDocument doc;
    doc["error"] = status == 400 ? "Perfil ou parametros invalidos" : status == 404 ? "Perfil inexistente" :
        status == 409 ? "Nome ja existe; carregue o perfil ou escolha outro nome" :
        status == 422 ? "Arquivo de perfil invalido" : status == 413 ? "Requisicao muito grande" :
        status == 507 ? "Limite de perfis ou falha de gravacao" : "Armazenamento ocupado ou indisponivel";
    reply(request, doc, status);
}
void encode(JsonDocument &doc, const KeyGauge::KeyGaugeProfile &p) {
    doc["name"] = p.name; doc["points"] = p.points; doc["thickness"] = p.thickness; doc["profileWidth"] = p.width;
    auto levels = doc["levels"].to<JsonArray>();
    for (int i = 0; i < p.points; ++i) levels.add(p.gaugePoints[i].level);
}
bool decode(AsyncWebServerRequest *request, KeyGauge::KeyGaugeProfile &p) {
    if (request->contentLength() > 2048) { error(request, 413); return false; }
    if (!request->hasParam("profile", true)) { error(request, 400); return false; }
    const String &body = request->getParam("profile", true)->value();
    JsonDocument doc;
    if (body.length() > 1024 || deserializeJson(doc, body) || !doc.is<JsonObject>() ||
        !doc["name"].is<const char *>() || !doc["points"].is<int>() ||
        !doc["thickness"].is<int>() || !doc["profileWidth"].is<int>() || !doc["levels"].is<JsonArray>()) {
        error(request, 400); return false;
    }
    String name = doc["name"].as<String>();
    int points = doc["points"], thickness = doc["thickness"], width = doc["profileWidth"];
    // Check wide integers before narrowing to uint8_t; never accept wrapped values.
    if (!KeyGauge::validName(name) || points < 4 || points > 10 || thickness < 1 || thickness > 10 ||
        width < 50 || width > 100 || doc["levels"].size() != size_t(points)) { error(request, 400); return false; }
    name.toCharArray(p.name, sizeof(p.name)); p.points = points; p.thickness = thickness; p.width = width;
    for (int i = 0; i < points; ++i) {
        JsonVariant level = doc["levels"][i];
        if (!level.is<int>() || level.as<int>() < 0 || level.as<int>() > 9) { error(request, 400); return false; }
        p.gaugePoints[i].level = level.as<int>();
    }
    if (!KeyGauge::validProfile(p)) { error(request, 400); return false; }
    return true;
}
}
void registerKeyGaugeWebApi(AsyncWebServer &server, bool (*authenticate)(AsyncWebServerRequest *)) {
    server.on("/api/keygauge/profiles", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return;
        std::vector<String> names; String next;
        int status = KeyGauge::listProfiles(names, next);
        if (status != 200) { error(request, status); return; }
        JsonDocument doc; auto items = doc["items"].to<JsonArray>();
        for (const auto &name : names) items.add(name);
        doc["nextName"] = next; reply(request, doc);
    });
    server.on("/api/keygauge/profile", HTTP_GET, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return;
        if (!request->hasParam("name")) { error(request, 400); return; }
        KeyGauge::KeyGaugeProfile p;
        int status = KeyGauge::readProfile(request->getParam("name")->value(), p);
        if (status != 200) { error(request, status); return; }
        JsonDocument doc; encode(doc, p); reply(request, doc);
    });
    server.on("/api/keygauge/profile", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return;
        KeyGauge::KeyGaugeProfile p; if (!decode(request, p)) return;
        bool replace = false;
        if (request->hasParam("replace", true)) {
            String value = request->getParam("replace", true)->value();
            if (value != "0" && value != "1") { error(request, 400); return; }
            replace = value == "1";
        }
        int status = KeyGauge::writeProfile(p, replace);
        if (status != 200) { error(request, status); return; }
        JsonDocument doc; encode(doc, p); reply(request, doc);
    });
    server.on("/api/keygauge/profile", HTTP_DELETE, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return;
        if (!request->hasParam("name")) { error(request, 400); return; }
        int status = KeyGauge::deleteProfile(request->getParam("name")->value());
        if (status != 200) { error(request, status); return; }
        JsonDocument doc; doc["message"] = "Perfil excluido"; reply(request, doc);
    });
    server.on("/api/keygauge/preview", HTTP_POST, [authenticate](AsyncWebServerRequest *request) {
        if (!authenticate(request)) return;
        KeyGauge::KeyGaugeProfile p; if (!decode(request, p)) return;
        KeyGauge::queueWebPreview(p);
        JsonDocument doc;
        doc["message"] = "Enviado em RAM. Na tela WebUI do dispositivo, o perfil aparece automaticamente. Em segundo plano: Ferramentas > Mali Tools > KEY GAUGE > PREVIA WEB. Nao salvo.";
        reply(request, doc, 202);
    });
}
