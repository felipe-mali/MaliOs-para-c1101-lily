#include "core/ui/PtBr.h"
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "counter_main.h"
namespace CounterLab {
enum Category : uint8_t { WIFI, BLE, RF, IR, NFC, SYSTEM, COUNT };
enum State : uint8_t { IDLE, SCANNING, CONFIGURING, RUNNING, STOPPING, COMPLETE, STOPPED, ERROR };
struct Config {
    Category category = SYSTEM;
    uint8_t mode = 0;
    bool simulation = true, authorized = false;
    uint32_t duration = 30, interval = 1000;
    char target[65] = {}, password[65] = {};
    float frequency = 433.92f;
    CounterSuite::Target selected;
};
struct Status {
    State state = IDLE;
    Config config;
    CounterSuite::Metrics metrics;
    uint32_t elapsed = 0, attempts = 0, sequence = 0;
    int16_t graph[60] = {}, rssiMin = 0, rssiMax = -127;
    int64_t rssiSum = 0;
    uint32_t rssiSamples = 0;
    uint8_t graphCount = 0;
    uint8_t bars[64] = {}, barCount = 0;
    uint8_t channelCount[14] = {};
    int16_t channelRssi[14] = {};
    char message[160] = {};
    Status() { strlcpy(message, MaliText::ready_20c7c5, sizeof(message)); }
};
const char *categoryName(Category category);
const char *modeName(Category category, uint8_t mode);
uint8_t modeCount(Category category);
bool available(Category category);
bool simulationOnly(Category category, uint8_t mode);
uint32_t minimumInterval(const Config &config);
const char *stateName(State state);
Status snapshot();
void describe(JsonDocument &doc);
void statusJson(JsonDocument &doc);
void targetsJson(JsonDocument &doc);
void historyJson(JsonDocument &doc);
// Queue validation only: AsyncTCP never runs hardware or draws the TFT.
bool request(const Config &config, bool scan, String &error);
void stop();
bool serviceDisplay();
bool hasPending();
void open();
bool saveResult(String &error);
void configureHistory(uint8_t limit);
}
