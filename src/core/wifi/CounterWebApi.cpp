#include "core/ui/PtBr.h"
#include "CounterWebApi.h"
#include "mali_tools/counter/CounterLab.h"
namespace {
void reply(AsyncWebServerRequest *r,JsonDocument &doc,int code=200){
    auto *response=r->beginResponseStream("application/json; charset=utf-8");
    if(!response){r->send(503,"text/plain",MaliText::out_of_memory_0be297);return;}
    response->setCode(code);response->addHeader("Cache-Control","no-store");serializeJson(doc,*response);r->send(response);
}
void error(AsyncWebServerRequest *r,int code,const String &message){JsonDocument doc;doc["error"]=message;reply(r,doc,code);}
bool decode(AsyncWebServerRequest *r,CounterLab::Config &c){
    if(r->contentLength()>2048){error(r,413,MaliText::request_exceeds_2048_bytes_0cb223);return false;}
    if(!r->hasParam("config",true)){error(r,400,MaliText::missing_config_json_4d0818);return false;}
    JsonDocument doc;
    if(deserializeJson(doc,r->getParam("config",true)->value())||!doc.is<JsonObject>() ||
        !doc["category"].is<int>() || !doc["mode"].is<int>() || !doc["duration"].is<int>() || !doc["interval"].is<int>() ||
        !doc["simulation"].is<bool>() || !doc["authorized"].is<bool>()) {error(r,400,MaliText::invalid_config_types_fe68f3);return false;}
    int category=doc["category"],mode=doc["mode"],duration=doc["duration"],interval=doc["interval"];
    if(category<0||category>=CounterLab::COUNT||mode<0||mode>=CounterLab::modeCount(CounterLab::Category(category))||duration<0||duration>3600||interval<100||interval>60000){error(r,400,MaliText::config_out_of_range_5c38dc);return false;}
    c.category=CounterLab::Category(category);c.mode=mode;c.duration=duration;c.interval=interval;c.simulation=doc["simulation"];c.authorized=doc["authorized"];
    if(doc["target"].is<const char*>()){
        String value=doc["target"].as<String>();if(value.length()>64||strlen(value.c_str())!=value.length()){error(r,400,MaliText::invalid_target_30f5d3);return false;}value.toCharArray(c.target,sizeof(c.target));
    }else if(!doc["target"].isNull()){error(r,400,MaliText::invalid_target_type_9c7334);return false;}
    if(doc["password"].is<const char*>()){
        String value=doc["password"].as<String>();if(value.length()>64||strlen(value.c_str())!=value.length()){error(r,400,MaliText::invalid_password_97ff68);return false;}value.toCharArray(c.password,sizeof(c.password));
    }else if(!doc["password"].isNull()){error(r,400,MaliText::invalid_password_type_5cd87a);return false;}
    if(!doc["frequency"].isNull()){
        if(!doc["frequency"].is<float>()){error(r,400,MaliText::invalid_rf_frequency_747f92);return false;}
        c.frequency=doc["frequency"].as<float>();if(!isfinite(c.frequency)){error(r,400,MaliText::invalid_rf_frequency_747f92);return false;}
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
            JsonDocument d;d["message"]=MaliText::queued_on_device_ui_cf00e7;reply(r,d,202);});
    }
    server.on("/api/counter/stop",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;CounterLab::stop();JsonDocument d;d["message"]=MaliText::stop_requested_db50fa;reply(r,d,202);});
    server.on("/api/counter/history",HTTP_GET,[auth](AsyncWebServerRequest *r){if(!auth(r))return;JsonDocument d;CounterLab::historyJson(d);reply(r,d);});
    server.on("/api/counter/save",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;String why;if(!CounterLab::saveResult(why)){error(r,409,why);return;}JsonDocument d;d["message"]=MaliText::summary_saved_90577f;reply(r,d);});
    server.on("/api/counter/settings",HTTP_POST,[auth](AsyncWebServerRequest *r){if(!auth(r))return;
        if(r->contentLength()>64||!r->hasParam("historyLimit",true)){error(r,400,MaliText::historylimit_required_bb3e38);return;}
        String value=r->getParam("historyLimit",true)->value();for(char c:value)if(c<'0'||c>'9'){error(r,400,MaliText::invalid_limit_ed7031);return;}
        int n=value.toInt();if(n<1||n>50){error(r,400,MaliText::limit_1_50_a93e1a);return;}CounterLab::configureHistory(n);JsonDocument d;d["message"]=MaliText::history_limit_updated_for_this_boot_33c200;reply(r,d);});
}
