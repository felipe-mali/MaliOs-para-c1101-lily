#include "CounterWebApi.h"
#include "mali_tools/counter/CounterLab.h"
namespace {
void reply(AsyncWebServerRequest *r,JsonDocument &doc,int code=200){
    auto *response=r->beginResponseStream("application/json; charset=utf-8");
    if(!response){r->send(503,"text/plain","Out of memory");return;}
    response->setCode(code);response->addHeader("Cache-Control","no-store");serializeJson(doc,*response);r->send(response);
}
void error(AsyncWebServerRequest *r,int code,const String &message){JsonDocument doc;doc["error"]=message;reply(r,doc,code);}
bool decode(AsyncWebServerRequest *r,CounterLab::Config &c){
    if(r->contentLength()>2048){error(r,413,"Request exceeds 2048 bytes");return false;}
    if(!r->hasParam("config",true)){error(r,400,"Missing config JSON");return false;}
    JsonDocument doc;
    if(deserializeJson(doc,r->getParam("config",true)->value())||!doc.is<JsonObject>() ||
        !doc["category"].is<int>() || !doc["mode"].is<int>() || !doc["duration"].is<int>() || !doc["interval"].is<int>() ||
        !doc["simulation"].is<bool>() || !doc["authorized"].is<bool>()) {error(r,400,"Invalid config types");return false;}
    int category=doc["category"],mode=doc["mode"],duration=doc["duration"],interval=doc["interval"];
    if(category<0||category>=CounterLab::COUNT||mode<0||mode>=CounterLab::modeCount(CounterLab::Category(category))||duration<0||duration>3600||interval<100||interval>60000){error(r,400,"Config out of range");return false;}
    c.category=CounterLab::Category(category);c.mode=mode;c.duration=duration;c.interval=interval;c.simulation=doc["simulation"];c.authorized=doc["authorized"];
    if(doc["target"].is<const char*>()){
        String value=doc["target"].as<String>();if(value.length()>64||strlen(value.c_str())!=value.length()){error(r,400,"Invalid target");return false;}value.toCharArray(c.target,sizeof(c.target));
    }else if(!doc["target"].isNull()){error(r,400,"Invalid target type");return false;}
    if(doc["password"].is<const char*>()){
        String value=doc["password"].as<String>();if(value.length()>64||strlen(value.c_str())!=value.length()){error(r,400,"Invalid password");return false;}value.toCharArray(c.password,sizeof(c.password));
    }else if(!doc["password"].isNull()){error(r,400,"Invalid password type");return false;}
    if(!doc["frequency"].isNull()){
        if(!doc["frequency"].is<float>()){error(r,400,"Invalid RF frequency");return false;}
        c.frequency=doc["frequency"].as<float>();if(!isfinite(c.frequency)){error(r,400,"Invalid RF frequency");return false;}
    }
    return true;
}
}
void registerCounterWebApi(AsyncWebServer &server,bool (*auth)(AsyncWebServerRequest *)){
    server.on("/api/counter/capabilities",HTTP_GET,[auth](AsyncWebServerRequest *r){if(!auth(r))return;JsonDocument d;CounterLab::describe(d);reply(r,d);});
    server.on("/api/counter/status",HTTP_GET,[auth](AsyncWebServerRequest *r){if(!auth(r))return;JsonDocument d;CounterLab::statusJson(d);reply(r,d);});
    server.on("/api/counter/targets",HTTP_GET,[auth](AsyncWebServerRequest *r){if(!auth(r))return;JsonDocument d;CounterLab::targetsJson(d);reply(r,d);});
    for(const char *path:{"/api/counter/start","/api/counter/scan"}){
        bool scan=String(path).endsWith("scan");
        server.on(path,HTTP_POST,[auth,scan](AsyncWebServerRequest *r){if(!auth(r))return;CounterLab::Config c;if(!decode(r,c))return;String why;
            bool ok=CounterLab::request(c,scan,why);memset(c.password,0,sizeof(c.password));if(!ok){error(r,409,why);return;}
            JsonDocument d;d["message"]="Queued on device UI";reply(r,d,202);});
    }
    server.on("/api/counter/stop",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;CounterLab::stop();JsonDocument d;d["message"]="STOP requested";reply(r,d,202);});
    server.on("/api/counter/history",HTTP_GET,[auth](AsyncWebServerRequest *r){if(!auth(r))return;JsonDocument d;CounterLab::historyJson(d);reply(r,d);});
    server.on("/api/counter/save",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;String why;if(!CounterLab::saveResult(why)){error(r,409,why);return;}JsonDocument d;d["message"]="Summary saved";reply(r,d);});
    server.on("/api/counter/settings",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;
        if(r->contentLength()>64||!r->hasParam("historyLimit",true)){error(r,400,"historyLimit required");return;}
        String value=r->getParam("historyLimit",true)->value();for(char c:value)if(c<'0'||c>'9'){error(r,400,"Invalid limit");return;}
        int n=value.toInt();if(n<1||n>50){error(r,400,"Limit 1..50");return;}CounterLab::configureHistory(n);JsonDocument d;d["message"]="History limit updated for this boot";reply(r,d);});
}
