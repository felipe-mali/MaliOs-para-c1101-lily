#include "core/ui/PtBr.h"
#include "core/radio_mem.h"
#include "counter_main.h"
#include "counter_metrics.h"
#include <soc/soc_caps.h>
#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif
namespace CounterSuite {
namespace {
#if SOC_BLE_SUPPORTED
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Device {
    char address[24] = {}, name[32] = {}, uuid[48] = {};
    uint32_t seen = 0, count = 0, total = 0;
    bool connectable = false;
    uint8_t addressType = 0, advType = 0;
    int rssi = -127;
    uint16_t vendor = 0xffff;
};
// Callback storage has static lifetime; it never references a destroyed monitor.
Device devices[64];
bool accepting = false;
uint32_t ads = 0, overflow = 0;
class Callbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *d) override {
        if (!d) return;
        Device value;
        snprintf(value.address, sizeof(value.address), "%s", d->getAddress().toString().c_str());
        snprintf(value.name, sizeof(value.name), "%s", d->getName().c_str());
        if (d->haveServiceUUID())
            snprintf(value.uuid, sizeof(value.uuid), "%s", d->getServiceUUID().toString().c_str());
        if (d->haveManufacturerData()) {
            auto bytes = d->getManufacturerData();
            if (bytes.size() >= 2) value.vendor = uint8_t(bytes[0]) | (uint16_t(uint8_t(bytes[1])) << 8);
        }
        value.rssi = d->getRSSI();
        value.connectable = d->isConnectable();
        value.addressType = d->getAddress().getType();
        value.advType = d->getAdvType();
        value.seen = millis();
        portENTER_CRITICAL(&mux);
        if (accepting) {
            ++ads;
            int slot = -1;
            for (int i = 0; i < 64; ++i)
                if (!strcmp(devices[i].address, value.address)) {
                    slot = i;
                    break;
                }
            if (slot < 0)
                for (int i = 0; i < 64; ++i)
                    if (!devices[i].address[0] || value.seen - devices[i].seen > 15000) {
                        slot = i;
                        break;
                    }
            if (slot >= 0) {
                value.count = !strcmp(devices[slot].address, value.address) ? devices[slot].count + 1 : 1;
                value.total = !strcmp(devices[slot].address, value.address) ? devices[slot].total + 1 : 1;
                devices[slot] = value;
            } else ++overflow;
        }
        portEXIT_CRITICAL(&mux);
    }
} callbacks;
class BleMonitor : public Monitor {
    NimBLEScan *scan = nullptr;
    bool owned = false;
    uint32_t at = 0, previous = 0;
    int previousActive = 0;
    char previousAddresses[64][24] = {};
    uint8_t previousUsed = 0;

public:
    size_t targets(Target *out, size_t capacity) override {
        size_t count = 0;
        portENTER_CRITICAL(&mux);
        for (const auto &d : devices) {
            if (!d.address[0] || count == capacity) continue;
            Target &t = out[count++]; t = Target();
            memcpy(t.name, d.name, sizeof(d.name)); memcpy(t.address, d.address, sizeof(t.address));
            memcpy(t.detail, d.uuid, sizeof(t.detail)); t.rssi = d.rssi;
            t.connectable = d.connectable; t.addressType = d.addressType; t.advType = d.advType;
        }
        portEXIT_CRITICAL(&mux);
        return count;
    }
    bool begin() override {
        if (NimBLEDevice::isInitialized() || radioLargestDmaBlock() < RADIO_BLE_MIN_DMA_BLOCK) return false;
        if (!NimBLEDevice::init("")) return false;
        owned = true;
        scan = NimBLEDevice::getScan();
        if (!scan) return false;
        portENTER_CRITICAL(&mux);
        for (auto &d : devices) d = Device();
        ads = overflow = 0;
        accepting = true;
        portEXIT_CRITICAL(&mux);
        scan->setActiveScan(false);
        scan->setInterval(sampleInterval ? min(uint32_t(1000),sampleInterval) : 80);
        scan->setWindow(60);
        scan->setDuplicateFilter(false);
        scan->setMaxResults(0);
        scan->setScanCallbacks(&callbacks, true);
        at = millis();
        data.status = "NORMAL";
        return scan->start(0, false, true);
    }
    void tick(uint32_t now) override {
        if (now - at < max(uint32_t(1000),sampleInterval)) return;
        Device strongest;
        int active = 0, high = 0, added = 0, gone = 0;
        char addresses[64][24] = {}, uuidList[3][48] = {};
        uint8_t uuidUsed = 0;
        uint32_t count, lost;
        portENTER_CRITICAL(&mux);
        count = ads;
        lost = overflow;
        if (target.length()) {
            for (const auto &d : devices) if (target == d.address) {
                metrics.events = metrics.rx = metrics.samples = d.total;
                metrics.rssi = d.rssi;
            }
        } else { metrics.events = metrics.rx = metrics.samples = ads; metrics.failures = overflow; }
        for (auto &d : devices) {
            if (d.address[0] && now - d.seen < 10000) {
                memcpy(addresses[active], d.address, 24);
                ++active;
                if (d.rssi > strongest.rssi) strongest = d;
                if (d.uuid[0] && uuidUsed < 3) {
                    bool found = false;
                    for (int u = 0; u < uuidUsed; ++u)
                        if (!strcmp(uuidList[u], d.uuid)) found = true;
                    if (!found) memcpy(uuidList[uuidUsed++], d.uuid, 48);
                }
            }
            if (perSecond(d.count, now - at) > 50) ++high;
            d.count = 0;
        }
        portEXIT_CRITICAL(&mux);
        if(!target.length())metrics.rssi=strongest.rssi;
        uint32_t rate = perSecond(count - previous, now - at);
        for (int i = 0; i < active; ++i) {
            bool known = false;
            for (int j = 0; j < previousUsed; ++j)
                if (!strcmp(addresses[i], previousAddresses[j])) known = true;
            if (!known) ++added;
        }
        for (int j = 0; j < previousUsed; ++j) {
            bool found = false;
            for (int i = 0; i < active; ++i)
                if (!strcmp(addresses[i], previousAddresses[j])) found = true;
            if (!found) ++gone;
        }
        memcpy(previousAddresses, addresses, sizeof(addresses));
        previousUsed = active;
        String status = rate > 300 && active > 20          ? MaliText::ble_flood_suspected_d2da64
                        : rate > 150                       ? MaliText::high_advertisement_rate_b4b6a2
                        : previousActive > 0 && added > 10 ? MaliText::device_spike_177c86
                                                           : "NORMAL";
        if (status != data.status && status != "NORMAL") event(BLE, status, true);
        data.status = status;
        data.warning = status != "NORMAL";
        data.lines[0] = MaliText::devices_10s_3dc35f + String(active) + " /64";
        data.lines[1] = MaliText::new_gone_79e272 + String(added) + " / " + String(gone);
        data.lines[2] = "Adv/s: " + String(rate) + " total: " + String(count);
        data.lines[3] = String(MaliText::strongest_1f9976) + (strongest.name[0] ? strongest.name : strongest.address);
        data.lines[4] = "RSSI: " + String(strongest.rssi) + " dBm";
        data.lines[5] = String("UUID: ") + strongest.uuid;
        data.lines[6] = String(MaliText::maker_c1fe4c) + (strongest.vendor == 0xffff   ? "N/A"
                                             : strongest.vendor == 0x004c ? "Apple"
                                             : strongest.vendor == 0x0075 ? "Samsung"
                                             : strongest.vendor == 0x0006 ? "Microsoft"
                                                                          : String(strongest.vendor, HEX));
        data.lines[7] = MaliText::devices_50_adv_s_92d3bd + String(high);
        data.lines[8] = MaliText::overflow_adv_270630 + String(lost);
        for (int u = 0; u < 3; ++u)
            data.lines[9 + u] = String("UUID sample ") + String(u + 1) + ": " + uuidList[u];
        at = now;
        previous = count;
        previousActive = active;
    }
    void end() override {
        if (!owned) return;
        portENTER_CRITICAL(&mux);
        accepting = false;
        portEXIT_CRITICAL(&mux);
        if (scan) {
            scan->stop();
            scan->setScanCallbacks(nullptr);
            scan->clearResults();
        }
        NimBLEDevice::deinit(true);
        owned = false;
        scan = nullptr;
    }
};
#else
class BleMonitor : public Monitor {
public:
    bool begin() override { return false; }
    void tick(uint32_t) override {}
    void end() override {}
};
#endif
} // namespace
std::unique_ptr<Monitor> makeBle() { return std::unique_ptr<Monitor>(new BleMonitor); }
} // namespace CounterSuite
