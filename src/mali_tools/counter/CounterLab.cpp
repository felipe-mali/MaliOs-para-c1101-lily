#include "core/ui/MaliUI.h"
#include "CounterLab.h"
#include "counter_metrics.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/led_control.h"
#include "core/radio_mem.h"
#include "modules/rf/rf_utils.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <soc/soc_caps.h>
#include <freertos/semphr.h>
#include <ping/ping_sock.h>
#include <lwip/ip_addr.h>
#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif
namespace CounterLab {
namespace {
const char *modes[COUNT][10] = {
    {"SCAN NETWORKS", "CONNECTION TEST", "RECONNECT TEST", "PACKET LOSS TEST", "LATENCY TEST", "THROUGHPUT SAMPLE", "SIGNAL STABILITY", "CHANNEL MONITOR", "SIMULATION MODE"},
    {"BLE SCANNER", "SIGNAL MONITOR", "ADVERTISEMENT MONITOR", "CONNECTION TEST", "RECONNECT TEST", "SERVICE ENUMERATION", "NOTIFICATION TEST", "SIMULATION MODE"},
    {"SPECTRUM MONITOR", "ACTIVITY COUNTER", "SIGNAL STRENGTH", "EVENT LOGGER", "SIMULATION"},
    {"IR MONITOR", "CAPTURE STATISTICS", "REPEAT TEST", "TIMING ANALYSIS", "SIMULATION"},
    {"SCAN", "READ STABILITY", "REPEATED READ TEST", "TIMING", "ERROR RATE", "SIMULATION"},
    {"TIMER / HEAP TEST", "SIMULATION"}
};
const uint8_t counts[] = {9,8,5,5,6,2};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Status published;
Config pending;
bool queued = false, queuedScan = false, stopping = false, inEngine = false, busy = false, foundSimulated = false;
uint32_t heartbeat = 0, sequence = 0;
CounterSuite::Target found[32];
uint8_t foundCount = 0;
Category foundCategory = SYSTEM;
uint32_t foundAt = 0;
uint8_t historyLimit = 20;
StaticSemaphore_t historyMutexControl;
SemaphoreHandle_t historyMutex = xSemaphoreCreateMutexStatic(&historyMutexControl);
struct HistoryLock {
    bool held = xSemaphoreTake(historyMutex, pdMS_TO_TICKS(200)) == pdTRUE;
    ~HistoryLock() { if (held) xSemaphoreGive(historyMutex); }
};
CounterSuite::Metrics pingMetrics;
uint32_t pingGeneration = 0;
bool pingAccepting = false;
void pingResult(esp_ping_handle_t handle, void *arg, bool success) {
    uint32_t time = 0;
    if (success) esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP, &time, sizeof(time));
    portENTER_CRITICAL(&mux);
    if (pingAccepting && uint32_t(reinterpret_cast<uintptr_t>(arg)) == pingGeneration) {
        ++pingMetrics.events; ++pingMetrics.tx;
        if (success) {
            ++pingMetrics.success; ++pingMetrics.rx; ++pingMetrics.samples;
            pingMetrics.timeTotal += time; pingMetrics.timeMin = min(pingMetrics.timeMin, time); pingMetrics.timeMax = max(pingMetrics.timeMax, time);
        } else ++pingMetrics.failures;
    }
    portEXIT_CRITICAL(&mux);
}
void publish(const Status &s) {
    portENTER_CRITICAL(&mux); published = s; memset(published.config.password,0,sizeof(published.config.password)); portEXIT_CRITICAL(&mux);
}
bool stopRequested() { portENTER_CRITICAL(&mux); bool result = stopping; portEXIT_CRITICAL(&mux); return result; }
bool connecting(const Config &c) { return (c.category == WIFI && (c.mode == 1 || c.mode == 2)) || (c.category == BLE && (c.mode == 3 || c.mode == 4)); }
bool pinging(const Config &c) { return c.category == WIFI && (c.mode == 3 || c.mode == 4); }
void sample(Status &s, int value) {
    if (s.graphCount < 60) s.graph[s.graphCount++] = value;
    else { memmove(s.graph, s.graph+1, 59*sizeof(s.graph[0])); s.graph[59] = value; }
}
void timedResult(Status &s, bool ok, uint32_t elapsed) {
    ++s.metrics.events;
    if (ok) {
        ++s.metrics.success; ++s.metrics.samples;
        s.metrics.timeTotal += elapsed; s.metrics.timeMin = min(s.metrics.timeMin, elapsed); s.metrics.timeMax = max(s.metrics.timeMax, elapsed);
    } else ++s.metrics.failures;
    const bool simulatedRssi=s.config.simulation&&(s.config.category==RF||s.config.category==BLE||s.config.category==WIFI)&&!connecting(s.config)&&!pinging(s.config);
    if(!simulatedRssi)sample(s, min(uint32_t(32767), elapsed));
}
void textLine(int y, const String &text, uint16_t color = TFT_LIGHTGREY) {
    tft.fillRect(10,y,tftWidth-20,11,MaliUI::SURFACE);
    tft.setTextColor(color, MaliUI::SURFACE);
    tft.drawString(text.substring(0, max(1, (tftWidth-24)/6)), 12, y, 1);
}
void draw(const Status &s, bool first) {
    const bool portrait=tftHeight>240;
    const bool outcomes=s.config.simulation||connecting(s.config)||pinging(s.config)||s.config.category==NFC||s.config.category==IR||s.config.category==SYSTEM;
    if(first){tft.fillScreen(MaliUI::BACKGROUND);MaliUI::drawHeader(categoryName(s.config.category));MaliUI::drawCard(4,28,tftWidth-8,tftHeight-46);}
    tft.setTextDatum(0);tft.setTextSize(1);
    textLine(33,String("TARGET: ")+(s.config.target[0]?s.config.target:"local / receiver"));
    textLine(46,modeName(s.config.category,s.config.mode));
    textLine(59,String(s.config.simulation?"SIM / ":"LAB / ")+stateName(s.state)+"  "+String(s.elapsed/1000)+"s",MaliUI::ACCENT);
    auto metric=[&](const char *label,const String &value,int x,int y,int width,uint16_t color){
        tft.fillRect(x,y,width,34,MaliUI::SURFACE);tft.setTextSize(1);tft.setTextColor(MaliUI::TEXT_SECONDARY,MaliUI::SURFACE);tft.drawString(label,x,y,1);
        tft.setTextSize(value.length()*12U<unsigned(width)?2:1);tft.setTextColor(color,MaliUI::SURFACE);tft.drawString(value,x,y+13,1);tft.setTextSize(1);
    };
    int step=(tftWidth-28)/3;
    if(portrait){
        metric("EVENTS",String(s.metrics.events),12,80,tftWidth-24,MaliUI::TEXT_PRIMARY);
        metric(outcomes?"SUCCESS":"RX",String(outcomes?s.metrics.success:s.metrics.rx),12,120,tftWidth-24,outcomes?MaliUI::SUCCESS:MaliUI::ACCENT);
        metric(outcomes?"FAIL":"RSSI dBm",String(outcomes?int32_t(s.metrics.failures):s.metrics.rssi),12,160,tftWidth-24,outcomes?MaliUI::ERROR:MaliUI::ACCENT);
        textLine(201,"RATE: "+String(s.elapsed?1000.0*s.metrics.events/s.elapsed:0,1)+"/s");
    }else{
        metric("EVENTS",String(s.metrics.events),12,77,step,MaliUI::TEXT_PRIMARY);
        metric(outcomes?"SUCCESS":"RX",String(outcomes?s.metrics.success:s.metrics.rx),12+step,77,step,outcomes?MaliUI::SUCCESS:MaliUI::ACCENT);
        metric(outcomes?"FAIL":"RSSI dBm",String(outcomes?int32_t(s.metrics.failures):s.metrics.rssi),12+2*step,77,step,outcomes?MaliUI::ERROR:MaliUI::ACCENT);
    }
    int top=portrait?218:130,bottom=tftHeight-35,height=max(3,bottom-top);
    tft.fillRect(12,top,tftWidth-24,height+1,MaliUI::SURFACE);
    if(s.barCount){
        int width=max(1,(tftWidth-24)/s.barCount);
        for(int i=0;i<s.barCount;++i){int h=s.bars[i]*height/100;tft.fillRect(12+i*width,bottom-h,max(1,width-1),h,MaliUI::ACCENT);}
    }else if(s.graphCount>1){
        int low=s.graph[0],high=s.graph[0];for(int i=1;i<s.graphCount;++i){low=min(low,int(s.graph[i]));high=max(high,int(s.graph[i]));}
        for(int i=1;i<s.graphCount;++i)tft.drawLine(12+(tftWidth-24)*(i-1)/59,bottom-(s.graph[i-1]-low)*height/max(1,high-low),12+(tftWidth-24)*i/59,bottom-(s.graph[i]-low)*height/max(1,high-low),MaliUI::ACCENT);
    }
    textLine(portrait?tftHeight-30:115,s.message,s.state==ERROR?MaliUI::ERROR:MaliUI::TEXT_SECONDARY);
    MaliUI::drawFooter("HOLD / CLICK / BACK: STOP");
}

void encode(JsonDocument &doc, const Status &s) {
    doc["hasTiming"] = s.config.simulation || connecting(s.config) || pinging(s.config) || s.config.category==NFC || s.config.category==SYSTEM || s.config.category==IR;
    doc["hasTx"] = s.config.simulation || pinging(s.config) || s.config.category==NFC;
    doc["hasOutcomes"] = s.config.simulation || connecting(s.config) || pinging(s.config) || s.config.category==NFC || s.config.category==SYSTEM || s.config.category==IR;
    doc["state"] = stateName(s.state); doc["category"] = s.config.category; doc["mode"] = s.config.mode;
    doc["modeName"] = modeName(s.config.category,s.config.mode); doc["simulation"] = s.config.simulation;
    doc["target"] = s.config.target; doc["duration"] = s.config.duration; doc["interval"] = s.config.interval;
    doc["elapsed"] = s.elapsed; doc["events"] = s.metrics.events; doc["success"] = s.metrics.success;
    doc["failures"] = s.metrics.failures; doc["retries"] = s.metrics.retries; doc["tx"] = s.metrics.tx; doc["rx"] = s.metrics.rx;
    doc["attempts"] = s.attempts; doc["samples"] = s.metrics.samples;
    doc["rate"] = s.elapsed ? s.metrics.events*1000.0/s.elapsed : 0;
    doc["successRate"] = s.metrics.success+s.metrics.failures ? 100.0*s.metrics.success/(s.metrics.success+s.metrics.failures) : 0;
    doc["avgTime"] = s.metrics.samples ? double(s.metrics.timeTotal)/s.metrics.samples : 0;
    doc["minTime"] = s.metrics.timeMin == UINT32_MAX ? 0 : s.metrics.timeMin; doc["maxTime"] = s.metrics.timeMax;
    doc["rssi"] = s.metrics.rssi; doc["rssiSamples"] = s.rssiSamples;
    doc["rssiMin"] = s.rssiMin; doc["rssiMax"] = s.rssiMax; doc["rssiAvg"] = s.rssiSamples?double(s.rssiSum)/s.rssiSamples:0;
    doc["message"] = s.message; doc["sequence"] = s.sequence;
    JsonArray graph=doc["graph"].to<JsonArray>(); for(int i=0;i<s.graphCount;++i)graph.add(s.graph[i]);
    JsonArray bars=doc["bars"].to<JsonArray>();for(int i=0;i<s.barCount;++i)bars.add(s.bars[i]);
    if(s.config.category==WIFI&&s.config.mode==7){auto channels=doc["channels"].to<JsonArray>();for(int i=0;i<14;++i){auto ch=channels.add<JsonObject>();ch["channel"]=i+1;ch["count"]=s.channelCount[i];ch["rssi"]=s.channelRssi[i];}}
}
void execute(Config c, bool scanOnly) {
    Status s; s.config=c; s.state=scanOnly?SCANNING:RUNNING; s.sequence=++sequence;
    std::unique_ptr<CounterSuite::Monitor> monitor;
    std::unique_ptr<CounterSuite::Target[]> targetBuffer;
    esp_ping_handle_t ping=nullptr;
#if SOC_BLE_SUPPORTED
    NimBLEClient *client=nullptr; bool bleOwned=false;
#endif
    bool wifiOwned=false, waiting=false, singleDone=false;
    const bool previousReconnect=WiFi.getAutoReconnect();
    uint32_t attemptAt=0, nextAt=0, start=millis(), renderAt=0, sampleAt=0, lastSamples=0, lastEvents=0;
    MaliLedStateGuard led(MaliLedState::MENU);
    auto fail=[&](const char *message){ s.state=ERROR;strlcpy(s.message,message,sizeof(s.message)); };
    const bool conn=connecting(c), pingMode=pinging(c);
    publish(s);draw(s,true);
    if (c.simulation) strlcpy(s.message,"SIMULATION: no external operations",sizeof(s.message));
    else if (conn && c.category==WIFI) {
        if(WiFi.getMode()!=WIFI_MODE_NULL) fail("Wi-Fi in use: local test requires Wi-Fi OFF");
        else if(!radioHasMemForWifi() || !WiFi.mode(WIFI_STA)) fail("Wi-Fi unavailable / memory");
        else { wifiOwned=true;WiFi.persistent(false);WiFi.setAutoReconnect(false); }
    } else if (conn && c.category==BLE) {
#if SOC_BLE_SUPPORTED
        if(NimBLEDevice::isInitialized() || radioLargestDmaBlock()<RADIO_BLE_MIN_DMA_BLOCK) fail("BLE in use / low memory");
        else if(!NimBLEDevice::init("")) fail("BLE init failed");
        else {
            bleOwned=true;client=NimBLEDevice::createClient();
            if(!client)fail("BLE client unavailable");
            else { client->setConnectTimeout(3000);client->setConnectRetries(0);client->setSelfDelete(false,false); }
        }
#endif
    } else if(pingMode) {
        ip_addr_t address;
        if(WiFi.status()!=WL_CONNECTED || !ipaddr_aton(c.target,&address)) fail("Connect Wi-Fi and set a unicast IPv4 host");
        else {
            esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG(); config.target_addr=address;config.count=ESP_PING_COUNT_INFINITE;
            config.interval_ms=c.interval;config.timeout_ms=500;config.data_size=32;
            portENTER_CRITICAL(&mux);pingMetrics=CounterSuite::Metrics();++pingGeneration;pingAccepting=true;uint32_t generation=pingGeneration;portEXIT_CRITICAL(&mux);
            esp_ping_callbacks_t callbacks={}; callbacks.cb_args=reinterpret_cast<void*>(uintptr_t(generation));
            callbacks.on_ping_success=[](esp_ping_handle_t h,void *a){pingResult(h,a,true);};
            callbacks.on_ping_timeout=[](esp_ping_handle_t h,void *a){pingResult(h,a,false);};
            if(esp_ping_new_session(&config,&callbacks,&ping)!=ESP_OK || esp_ping_start(ping)!=ESP_OK)fail("Ping could not start");
        }
    } else if(c.category!=SYSTEM) {
        switch(c.category){
            case WIFI:monitor=CounterSuite::makeWifi(true);break;
            case BLE:monitor=CounterSuite::makeBle();break;
            case RF:CounterSuite::setSpectrumCenter(c.frequency);monitor=CounterSuite::makeRf(false);break;
            case IR:monitor=CounterSuite::makeIr();break;
            case NFC:monitor=CounterSuite::makeNfc();break;
            default:break;
        }
        if(monitor){monitor->target=c.target;monitor->sampleInterval=c.interval;if(!monitor->begin())fail("Hardware unavailable / already in use");}
    }
    if(scanOnly || (c.category==WIFI && c.mode==7)){
        targetBuffer.reset(new CounterSuite::Target[32]);
        portENTER_CRITICAL(&mux);foundCount=0;foundCategory=c.category;foundAt=millis();foundSimulated=c.simulation;
        if(c.simulation){
            foundCount=3;
            for(int i=0;i<3;++i){found[i]=CounterSuite::Target();snprintf(found[i].name,sizeof(found[i].name),"SIM_TARGET_%d",i+1);snprintf(found[i].address,sizeof(found[i].address),"02:00:00:00:00:%02d",i+1);found[i].rssi=-40-i*12;found[i].channel=1+i*5;found[i].connectable=true;}
        }
        portEXIT_CRITICAL(&mux);
    }
    check(SelPress);check(EscPress);
    while(s.state==RUNNING || s.state==SCANNING){
        uint32_t now=millis();s.elapsed=now-start;
        // A short click also stops; holding the encoder can never be less effective.
        if(stopRequested() || check(EscPress) || (check(SelPress)&&s.elapsed>350)){s.state=STOPPING;publish(s);break;}
        if((scanOnly && s.elapsed>=6000) || (c.duration && s.elapsed>=c.duration*1000U) || singleDone){s.state=COMPLETE;break;}
        if(c.simulation){
            if(now-sampleAt>=c.interval){sampleAt=now;++s.attempts;++s.metrics.tx;
                bool ok=s.attempts%11!=0;timedResult(s,ok,20+(s.attempts*37)%230);if(ok)++s.metrics.rx;
                s.metrics.retries=s.attempts? s.attempts-1:0;
                if((c.category==RF||c.category==BLE||c.category==WIFI)&&!conn&&!pingMode)s.metrics.rssi=-40-(s.attempts*7)%45;
            }
        }else if(conn){
            if(!waiting && int32_t(now-nextAt)>=0){
                waiting=true;attemptAt=now;++s.attempts;s.metrics.retries=s.attempts-1;
                if(c.category==WIFI) WiFi.begin(c.selected.name,c.password,c.selected.channel);
#if SOC_BLE_SUPPORTED
                else if(client && !client->connect(NimBLEAddress(c.target,c.selected.addressType),true,true,false)){
                    timedResult(s,false,0);waiting=false;nextAt=now+c.interval;
                }
#endif
            }
            if(waiting){
                bool connected=false;
                if(c.category==WIFI) connected=WiFi.status()==WL_CONNECTED;
#if SOC_BLE_SUPPORTED
                else if(client) connected=client->isConnected();
#endif
                if(connected || now-attemptAt >= (c.category==WIFI?10000U:3500U)){
                    timedResult(s,connected,now-attemptAt);waiting=false;nextAt=now+c.interval;
                    if(c.category==WIFI) WiFi.disconnect(false,false);
#if SOC_BLE_SUPPORTED
                    else if(client){if(connected)client->disconnect();else client->cancelConnect();}
#endif
                    singleDone=(c.category==WIFI?c.mode==1:c.mode==3);
                }
            }
        }else if(pingMode){portENTER_CRITICAL(&mux);s.metrics=pingMetrics;portEXIT_CRITICAL(&mux);s.attempts=s.metrics.tx;
        }else if(monitor){
            monitor->tick(now);s.metrics=monitor->metrics;strlcpy(s.message,monitor->data.lines[c.category==IR?1:0].c_str(),sizeof(s.message));
            if(c.category==NFC)s.attempts=s.metrics.events;
            if((c.category==IR||c.category==NFC)&&s.metrics.events!=lastEvents){lastEvents=s.metrics.events;if(s.metrics.samples)sample(s,min(uint32_t(32767),s.metrics.timeTotal/s.metrics.samples));}
            if((c.category==WIFI&&c.mode==7)||(c.category==RF&&c.mode==0)){
                s.barCount=min(uint8_t(64),monitor->data.barCount);
                for(int i=0;i<s.barCount;++i)s.bars[i]=constrain(monitor->data.bars[i],0,100);
            }
            if(targetBuffer && now-sampleAt>=1000){
                sampleAt=now;
                size_t count=monitor->targets(targetBuffer.get(),32);
                portENTER_CRITICAL(&mux);foundCount=count;for(size_t i=0;i<count;++i)found[i]=targetBuffer[i];foundAt=now;portEXIT_CRITICAL(&mux);
                memset(s.channelCount,0,sizeof(s.channelCount));int sums[14]={};
                for(size_t i=0;i<count;++i){auto &t=targetBuffer[i];if(t.channel>=1&&t.channel<=14){++s.channelCount[t.channel-1];sums[t.channel-1]+=t.rssi;}}
                for(int i=0;i<14;++i)s.channelRssi[i]=s.channelCount[i]?sums[i]/s.channelCount[i]:-127;
            }
        }else if(c.category==SYSTEM && now-sampleAt>=c.interval){
            uint32_t jitter=sampleAt?abs(int32_t(now-sampleAt-c.interval)):0;sampleAt=now;++s.attempts;
            timedResult(s,ESP.getFreeHeap()>16384,jitter);snprintf(s.message,sizeof(s.message),"Heap:%u min:%u / timer jitter ms",unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));
        }
        if(s.metrics.samples!=lastSamples && s.metrics.rssi!=-127){lastSamples=s.metrics.samples;++s.rssiSamples;
            s.rssiMin=min(s.rssiMin,s.metrics.rssi);s.rssiMax=max(s.rssiMax,s.metrics.rssi);s.rssiSum+=s.metrics.rssi;sample(s,s.metrics.rssi);
        }else if(pingMode && now-sampleAt>=1000){sampleAt=now;sample(s,s.metrics.samples?s.metrics.timeTotal/s.metrics.samples:0);}
        if(now-renderAt>=200){renderAt=now;publish(s);draw(s,renderAt==0);}
        delay(5);
    }
    s.elapsed=millis()-start;
    bool cancelled=s.state==STOPPING;
    if(monitor)monitor->end();
    if(ping){portENTER_CRITICAL(&mux);pingAccepting=false;portEXIT_CRITICAL(&mux);esp_ping_stop(ping);esp_ping_delete_session(ping);}
    if(wifiOwned){WiFi.disconnect(false,false);WiFi.setAutoReconnect(previousReconnect);WiFi.mode(WIFI_MODE_NULL);}
#if SOC_BLE_SUPPORTED
    if(bleOwned){if(client){client->cancelConnect();if(client->isConnected())client->disconnect();}NimBLEDevice::deinit(true);}
#endif
    memset(c.password,0,sizeof(c.password));memset(s.config.password,0,sizeof(s.config.password));
    if(cancelled)s.state=STOPPED;
    if(scanOnly && s.state==COMPLETE){s.state=CONFIGURING;strlcpy(s.message,"Scan complete: select a target",sizeof(s.message));}
    else if(s.state==COMPLETE)strlcpy(s.message,"TEST COMPLETE / result in RAM",sizeof(s.message));
    else if(s.state==STOPPED)strlcpy(s.message,"STOPPED / resources released",sizeof(s.message));
    publish(s);draw(s,true);
    portENTER_CRITICAL(&mux);busy=false;stopping=false;portEXIT_CRITICAL(&mux);
}
}
const char *categoryName(Category c){static const char *names[]={"Wi-Fi","Bluetooth / BLE","RF","Infrared","NFC","System Test"};return c<COUNT?names[c]:"Unknown";}
const char *modeName(Category c,uint8_t m){return c<COUNT&&m<counts[c]?modes[c][m]:"Unknown";}
uint8_t modeCount(Category c){return c<COUNT?counts[c]:0;}
const char *stateName(State s){static const char *names[]={"IDLE","SCANNING","CONFIGURING","RUNNING","STOPPING","COMPLETE","STOPPED","ERROR"};return s<=ERROR?names[s]:"ERROR";}
bool available(Category c){
    if(c==WIFI || c==SYSTEM)return true;
    if(c==BLE)return SOC_BLE_SUPPORTED;
    if(c==IR)return bruceConfigPins.irRx!=GPIO_NUM_NC;
#if defined(USE_CC1101_VIA_SPI)
    if(c==RF)return bruceConfigPins.rfModule==CC1101_SPI_MODULE && bruceConfigPins.CC1101_bus.cs!=GPIO_NUM_NC;
#endif
#if !defined(REMOVE_RFID_HW_INTERFACE)
    if(c==NFC)return bruceConfigPins.rfidModule==PN532_I2C_MODULE || bruceConfigPins.rfidModule==PN532_SPI_MODULE;
#endif
    return false;
}
bool simulationOnly(Category c,uint8_t m){return (c==WIFI&&m==5)||(c==BLE&&(m==5||m==6))||(c==IR&&m==2)||m==modeCount(c)-1;}
uint32_t minimumInterval(const Config &c){
    if(connecting(c))return 2000;
    if(pinging(c))return 500;
    if(c.category==WIFI)return 4000;
    if(c.category==BLE)return 1000;
    return c.category==NFC?250:100;
}
Status snapshot(){portENTER_CRITICAL(&mux);Status s=published;portEXIT_CRITICAL(&mux);return s;}
void statusJson(JsonDocument &doc){encode(doc,snapshot());portENTER_CRITICAL(&mux);bool isQueued=queued;portEXIT_CRITICAL(&mux);doc["pending"]=isQueued;}
void describe(JsonDocument &doc){
    JsonArray categories=doc["categories"].to<JsonArray>();
    for(int i=0;i<COUNT;++i)if(available(Category(i))){auto cat=categories.add<JsonObject>();cat["id"]=i;cat["name"]=categoryName(Category(i));auto list=cat["modes"].to<JsonArray>();
        for(int j=0;j<modeCount(Category(i));++j){auto mode=list.add<JsonObject>();mode["id"]=j;mode["name"]=modeName(Category(i),j);mode["simulationOnly"]=simulationOnly(Category(i),j);Config c;c.category=Category(i);c.mode=j;mode["minInterval"]=minimumInterval(c);}}
    doc["historyLimit"]=historyLimit;
}
void targetsJson(JsonDocument &doc){
    std::unique_ptr<CounterSuite::Target[]> copy(new CounterSuite::Target[32]);
    portENTER_CRITICAL(&mux);uint8_t count=foundCount;Category category=foundCategory;for(int i=0;i<count;++i)copy[i]=found[i];portEXIT_CRITICAL(&mux);
    doc["category"]=category;auto items=doc["items"].to<JsonArray>();
    for(int i=0;i<count;++i){auto item=items.add<JsonObject>();auto &t=copy[i];item["name"]=t.name;item["address"]=t.address;item["rssi"]=t.rssi;item["channel"]=t.channel;item["securityOrService"]=t.detail;item["connectable"]=t.connectable;item["advType"]=t.advType;}
}
bool request(const Config &config,bool scan,String &error){
    Config c=config;
    if(c.category>=COUNT || !available(c.category) || c.mode>=modeCount(c.category) || c.duration>3600 || c.interval<minimumInterval(c) || c.interval>60000){error="Invalid category/mode; duration 0..3600 s, interval within mode limits";return false;}
    if(!c.simulation && simulationOnly(c.category,c.mode)){error="This mode requires SIMULATION (no bounded/cancellable lab adapter)";return false;}
    if(!c.simulation && !scan && (connecting(c)||pinging(c)||c.category==NFC) && !c.authorized){error="Confirm LAB / AUTHORIZED TARGETS ONLY";return false;}
    if(c.category==RF&&(!isfinite(c.frequency)||!CounterSuite::validRfFrequency(c.frequency-.2f)||!CounterSuite::validRfFrequency(c.frequency+.2f))){error="Unsupported CC1101 frequency";return false;}
    if(scan && c.category!=WIFI&&c.category!=BLE){error="Scan selection is available for Wi-Fi/BLE; use receiver test for other protocols";return false;}
    if(!c.simulation && pinging(c)){
        IPAddress ip;
        if(!ip.fromString(c.target)||ip[0]==0||ip[0]==127||ip[0]>=224||ip[3]==0||ip[3]==255){error="Set an explicit unicast IPv4 host";return false;}
    }
    portENTER_CRITICAL(&mux);
    bool occupied=busy||queued||inEngine;
    bool ready=millis()-heartbeat<1500;
    bool selected=false;
    if(!c.simulation && !scan && (connecting(c)||(c.category==BLE&&c.mode!=0))){
        if(!foundSimulated && foundCategory==c.category && millis()-foundAt<120000)for(int i=0;i<foundCount;++i)if(!strcmp(c.target,found[i].address)){c.selected=found[i];selected=true;}
    }else selected=true;
    portEXIT_CRITICAL(&mux);
    if(occupied || !ready){error="Busy: return device to a menu/WebUI screen before START";return false;}
    if(!selected){error="SCAN and SELECT a recent target first";return false;}
    if(!c.simulation && connecting(c) && c.category==BLE&&!c.selected.connectable){error="Selected BLE advertisement is not connectable";return false;}
    if(!c.simulation && connecting(c) && c.category==WIFI && (!c.selected.name[0] || (strcmp(c.selected.detail,"OPEN") && strlen(c.password)<8))){error="Set the selected SSID and its Wi-Fi password (8..64 bytes)";return false;}
    if(!c.simulation && connecting(c)&&c.category==WIFI&&WiFi.getMode()!=WIFI_MODE_NULL){error="Connection test needs Wi-Fi OFF; use device menu or SIMULATION while WebUI is active";return false;}
    portENTER_CRITICAL(&mux);
    if(busy||queued){portEXIT_CRITICAL(&mux);error="A test is already pending";return false;}
    pending=c;queued=true;queuedScan=scan;published=Status();published.config=c;memset(published.config.password,0,sizeof(published.config.password));published.state=CONFIGURING;stopping=false;
    portEXIT_CRITICAL(&mux);return true;
}
void stop(){portENTER_CRITICAL(&mux);stopping=true;if(busy)published.state=STOPPING;if(queued){queued=false;memset(pending.password,0,sizeof(pending.password));published.state=STOPPED;}portEXIT_CRITICAL(&mux);}
bool hasPending(){portENTER_CRITICAL(&mux);bool value=queued;portEXIT_CRITICAL(&mux);return value;}
bool serviceDisplay(){
    portENTER_CRITICAL(&mux);heartbeat=millis();
    if(inEngine || !queued){portEXIT_CRITICAL(&mux);return false;}
    Config c=pending;bool scan=queuedScan;memset(pending.password,0,sizeof(pending.password));queued=false;busy=true;inEngine=true;portEXIT_CRITICAL(&mux);
    execute(c,scan);
    memset(c.password,0,sizeof(c.password));
    portENTER_CRITICAL(&mux);inEngine=false;heartbeat=millis();portEXIT_CRITICAL(&mux);
    return true;
}
void configureHistory(uint8_t limit){if(limit>=1&&limit<=50)historyLimit=limit;}
bool saveResult(String &error){
    Status s=snapshot();if(s.state!=COMPLETE&&s.state!=STOPPED){error="Finish or stop a test first";return false;}
    HistoryLock lock;if(!lock.held){error="History busy";return false;}
    FS &fs=sdcardMounted?static_cast<FS&>(SD):static_cast<FS&>(LittleFS);
    if((!fs.exists("/MaliTools")&&!fs.mkdir("/MaliTools"))||(!fs.exists("/MaliTools/Counter")&&!fs.mkdir("/MaliTools/Counter"))){error="Storage unavailable";return false;}
    char path[64];snprintf(path,sizeof(path),"/MaliTools/Counter/result_%02u.json",unsigned(s.sequence%historyLimit));
    String temp=String(path)+".tmp";JsonDocument doc;encode(doc,s);doc.remove("graph");doc.remove("bars");doc.remove("channels");
    File f=fs.open(temp,FILE_WRITE);if(!f){error="Cannot open result";return false;}
    size_t size=measureJson(doc),written=serializeJson(doc,f);f.flush();bool ok=written==size&&!f.getWriteError();f.close();
    if(!ok){fs.remove(temp);error="Result write failed";return false;}
    if(fs.exists(path)&&!fs.remove(path)){fs.remove(temp);error="Cannot replace history slot";return false;}
    if(!fs.rename(temp,path)){error="Result commit failed";return false;}
    // Shrinking the setting also bounds older slots on the next explicit save.
    for(int i=historyLimit;i<50;++i){char old[64];snprintf(old,sizeof(old),"/MaliTools/Counter/result_%02d.json",i);if(fs.exists(old))fs.remove(old);}
    return true;
}
void historyJson(JsonDocument &doc){
    HistoryLock lock;auto list=doc["items"].to<JsonArray>();if(!lock.held){doc["error"]="History busy";return;}
    FS &fs=sdcardMounted?static_cast<FS&>(SD):static_cast<FS&>(LittleFS);
    for(int i=0;i<50;++i){char path[64];snprintf(path,sizeof(path),"/MaliTools/Counter/result_%02d.json",i);File f=fs.open(path,FILE_READ);if(!f||f.size()>2048)continue;JsonDocument entry;if(!deserializeJson(entry,f))list.add(entry.as<JsonVariant>());}
}
namespace {
void localRun(Config c,bool scan=false){
    bool repeat;
    do {
    repeat=false;
    portENTER_CRITICAL(&mux);heartbeat=millis();portEXIT_CRITICAL(&mux);
    String error;if(!request(c,scan,error)){displayError(error,true);return;}serviceDisplay();
    if(scan)return;
    bool again=false;std::vector<Option> results={
        {"RESULTS",[](){Status s=snapshot();JsonDocument doc;encode(doc,s);std::vector<Option> rows;for(const char *key:{"state","modeName","target","elapsed","events","attempts","success","failures","successRate","avgTime","minTime","maxTime"}){String line=String(key)+": "+doc[key].as<String>();rows.push_back({line,[line](){displayInfo(line,true);}});}rows.push_back({"BACK",[](){}});loopOptions(rows,MENU_TYPE_SUBMENU,"RESULTS / time in ms");}},
        {"RUN AGAIN",[&](){again=true;}},{"SAVE RESULT",[](){String error;if(!saveResult(error))displayError(error,true);else displayInfo("Result saved",true);}},{"BACK",[](){}}
    };
    loopOptions(results,MENU_TYPE_SUBMENU,"COUNTER / RESULTS");repeat=again;
    } while(repeat && !returnToMenu);
}
void configure(Config &c){
    bool done=false;
    while(!done&&!returnToMenu){std::vector<Option> options={
        {"MODE",[&](){std::vector<Option> modesMenu;for(int i=0;i<modeCount(c.category);++i)modesMenu.push_back({String(modeName(c.category,i))+(simulationOnly(c.category,i)?" [SIM]":""),[&,i](){c.mode=i;if(simulationOnly(c.category,i))c.simulation=true;c.interval=max(c.interval,minimumInterval(c));}});loopOptions(modesMenu,MENU_TYPE_SUBMENU,"MODE");}},
        {String("SIMULATION: ")+(c.simulation?"ON":"OFF"),[&](){c.simulation=!c.simulation;if(simulationOnly(c.category,c.mode))c.simulation=true;}},
        {"TARGET / HOST",[&](){String value=keyboard(c.target,64,"Target address / IPv4 host");if(value!="\x1B")value.toCharArray(c.target,sizeof(c.target));}},
        {"WIFI PASSWORD",[&](){String value=keyboard("",64,"Lab Wi-Fi password",true);if(value!="\x1B")value.toCharArray(c.password,sizeof(c.password));}},
        {"DURATION",[&](){std::vector<Option> list;for(int n:{5,10,30,60,0})list.push_back({n?String(n)+" sec":"Unlimited",[&,n](){c.duration=n;}});list.push_back({"Custom 1..3600",[&](){int n=num_keyboard("30",4,"Seconds").toInt();if(n>=1&&n<=3600)c.duration=n;}});loopOptions(list,MENU_TYPE_SUBMENU,"DURATION");}},
        {String("INTENSITY: ")+String(c.interval)+"ms",[&](){std::vector<Option> list;for(int factor:{4,2,1}){uint32_t interval=min(uint32_t(60000),minimumInterval(c)*factor);list.push_back({String(factor==4?"LOW ":factor==2?"MEDIUM ":"HIGH ")+String(interval)+"ms",[&,interval](){c.interval=interval;}});}list.push_back({"CUSTOM",[&](){int n=num_keyboard(String(c.interval),5,"Interval ms").toInt();if(n>=int(minimumInterval(c))&&n<=60000)c.interval=n;}});loopOptions(list,MENU_TYPE_SUBMENU,"INTENSITY / POLLING");}},
        {"RF CENTER MHz",[&](){String f=keyboard(String(c.frequency,3),12,"CC1101 center MHz");float n=f.toFloat();if(CounterSuite::validRfFrequency(n))c.frequency=n;}},
        {"LAB / AUTHORIZED TARGETS ONLY",[&](){c.authorized=true;}},
        {"RUN",[&](){localRun(c);}},{"BACK",[&](){done=true;}}
    };if(loopOptions(options,MENU_TYPE_SUBMENU,"COUNTER / CONFIGURE")<0)break;}
    memset(c.password,0,sizeof(c.password));
}
void categoryMenu(Category category){
    Config c;c.category=category;c.interval=max(c.interval,minimumInterval(c));bool done=false;
    while(!done&&!returnToMenu){std::vector<Option> options;
        if(category==WIFI||category==BLE)options.push_back({"SCAN / SELECT",[&](){Config scan=c;scan.mode=0;scan.duration=10;scan.interval=minimumInterval(scan);localRun(scan,true);
            std::vector<CounterSuite::Target> copy(32);portENTER_CRITICAL(&mux);uint8_t count=foundCount;for(int i=0;i<count;++i)copy[i]=found[i];portEXIT_CRITICAL(&mux);
            std::vector<Option> list;for(int i=0;i<count;++i){auto t=copy[i];list.push_back({String(t.name[0]?t.name:t.address)+" "+String(t.rssi)+"dBm",[&,t](){c.selected=t;strlcpy(c.target,t.address,sizeof(c.target));displayInfo(String(t.address)+" CH:"+String(t.channel)+" "+t.detail+" Adv:"+String(t.advType)+(t.connectable?" connectable":""),true);}});}list.push_back({"BACK",[](){}});loopOptions(list,MENU_TYPE_SUBMENU,"SELECT / max 32");}});
        options.push_back({"CONFIGURE / RUN",[&](){configure(c);}});options.push_back({"SIMULATION MODE",[&](){c.simulation=true;configure(c);}});options.push_back({"BACK",[&](){done=true;}});
        if(loopOptions(options,MENU_TYPE_SUBMENU,categoryName(category))<0)break;
    }
}
}
void open(){
    bool done=false;
    while(!done&&!returnToMenu){std::vector<Option> options;
        for(int i=0;i<COUNT;++i)if(available(Category(i)))options.push_back({categoryName(Category(i)),[i](){categoryMenu(Category(i));}});
        options.push_back({"History",[](){JsonDocument doc;historyJson(doc);std::vector<Option> list;for(JsonObject item:doc["items"].as<JsonArray>()){String line=String(item["modeName"].as<const char*>())+" "+String(item["events"].as<uint32_t>())+" events";list.push_back({line,[line](){displayInfo(line,true);}});}list.push_back({"BACK",[](){}});loopOptions(list,MENU_TYPE_SUBMENU,"HISTORY / SUMMARY");}});
        options.push_back({"Settings",[](){int n=num_keyboard(String(historyLimit),2,"History limit 1..50").toInt();configureHistory(n);}});
        options.push_back({"Back",[&](){done=true;}});
        if(loopOptions(options,MENU_TYPE_GEAR,"COUNTER")<0)break;
    }
}
}

