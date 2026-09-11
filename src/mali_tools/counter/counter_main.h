#pragma once
#include <Arduino.h>
#include <memory>
namespace CounterSuite {
enum Module : uint8_t { WIFI, BLE, RF, NFC, IR, NRF, COUNT };
struct Target {
    char name[33] = {}, address[24] = {}, detail[48] = {};
    int16_t rssi = -127;
    uint8_t channel = 0, addressType = 0, advType = 0;
    bool connectable = false;
};
struct Metrics {
    uint32_t events = 0, success = 0, failures = 0, retries = 0, tx = 0, rx = 0;
    uint32_t samples = 0, timeTotal = 0, timeMin = UINT32_MAX, timeMax = 0;
    int16_t rssi = -127;
};
struct Snapshot {
    String status = "IDLE";
    String lines[12];
    uint8_t bars[80] = {};
    uint8_t barCount = 0;
    uint32_t updated = 0;
    bool warning = false;
};
class Monitor {
public:
    Snapshot data;
    Metrics metrics;
    String target;
    uint32_t sampleInterval = 0;
    virtual size_t targets(Target *, size_t) { return 0; }
    virtual ~Monitor() = default;
    virtual bool begin() = 0;
    virtual void tick(uint32_t now) = 0;
    virtual void end() = 0;
    virtual void save() {}
};
std::unique_ptr<Monitor> makeWifi(bool shareConnection = false);
std::unique_ptr<Monitor> makeBle();
std::unique_ptr<Monitor> makeRf(bool watch = false);
std::unique_ptr<Monitor> makeNfc();
std::unique_ptr<Monitor> makeIr();
std::unique_ptr<Monitor> makeNrf();
bool nrfAvailable();
void open();
void run(Module module, bool global = false, bool watch = false);
void rfMenu();
void setSpectrumCenter(float mhz);
void event(Module module, const String &message, bool warning = false);
const char *name(Module module);
} // namespace CounterSuite
