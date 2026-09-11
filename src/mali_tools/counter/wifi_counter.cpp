#include "core/radio_mem.h"
#include "counter_main.h"
#include "counter_metrics.h"
#include <WiFi.h>
#include <esp_wifi.h>
namespace CounterSuite {
namespace {
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Traffic {
    uint32_t packets = 0, management = 0, deauth = 0, channels[14] = {};
    uint8_t addresses[64][6] = {};
    uint8_t used = 0;
} traffic;
bool listening = false;
void receive(void *buffer, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA && type != WIFI_PKT_CTRL) return;
    auto *pkt = static_cast<wifi_promiscuous_pkt_t *>(buffer);
    if (pkt->rx_ctrl.sig_len < (type == WIFI_PKT_CTRL ? 10 : 24) || pkt->rx_ctrl.rx_state != 0) return;
    portENTER_CRITICAL(&mux);
    if (listening) {
        ++traffic.packets;
        if (type == WIFI_PKT_MGMT) {
            ++traffic.management;
            uint8_t subtype = pkt->payload[0] >> 4;
            if (subtype == 12 || subtype == 10) ++traffic.deauth;
        }
        uint8_t ch = pkt->rx_ctrl.channel;
        if (ch >= 1 && ch <= 14) ++traffic.channels[ch - 1];
        if (type == WIFI_PKT_CTRL) {
            portEXIT_CRITICAL(&mux);
            return;
        }
        const uint8_t *mac = pkt->payload + 10;
        bool known = false;
        for (int i = 0; i < traffic.used; ++i)
            if (!memcmp(mac, traffic.addresses[i], 6)) known = true;
        if (!known && traffic.used < 64 && !(mac[0] & 1)) memcpy(traffic.addresses[traffic.used++], mac, 6);
    }
    portEXIT_CRITICAL(&mux);
}
class WifiMonitor : public Monitor {
    bool owned = false, scanning = false, hooked = false, shared = false;
    Target found[32];
    size_t foundCount = 0;
    uint32_t scanAt = 0, window = 0, previous = 0, previousDeauth = 0;
    float baseline = 0;
    uint32_t previousChannels[14] = {};
    uint8_t previousMac[64][6] = {};
    int previousCount = 0;
    bool haveScan = false, churn = false;
    uint32_t churnAt = 0;

public:
    explicit WifiMonitor(bool share) : shared(share) {}
    size_t targets(Target *out, size_t capacity) override {
        size_t count = min(capacity, foundCount);
        for (size_t i = 0; i < count; ++i) out[i] = found[i];
        return count;
    }
    bool begin() override {
        // Refuse an existing session: never replace another tool's callback or stop its AP.
        if (WiFi.getMode() != WIFI_MODE_NULL && shared) {
            if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) { data.status = "SCAN IN USE"; return false; }
            scanAt = millis() - max(uint32_t(4000),sampleInterval);
            window = millis();
            data.status = "PASSIVE SCAN / SHARED WIFI";
            data.barCount = 14;
            return true;
        }
        if (WiFi.getMode() != WIFI_MODE_NULL) {
            data.status = "WIFI IN USE";
            data.lines[0] = "Desligue Wi-Fi no menu Rede";
            return false;
        }
        if (!radioHasMemForWifi()) {
            data.status = "LOW MEMORY";
            return false;
        }
        if (!WiFi.mode(WIFI_STA)) return false;
        owned = true;
        portENTER_CRITICAL(&mux);
        traffic = Traffic();
        listening = true;
        portEXIT_CRITICAL(&mux);
        wifi_promiscuous_filter_t filter{};
        filter.filter_mask =
            WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
        if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK ||
            esp_wifi_set_promiscuous_rx_cb(receive) != ESP_OK)
            return false;
        hooked = true;
        if (esp_wifi_set_promiscuous(true) != ESP_OK) return false;
        window = millis();
        scanAt = window - max(uint32_t(4000),sampleInterval);
        data.status = "NORMAL";
        data.barCount = 14;
        return true;
    }
    void tick(uint32_t now) override {
        if (!scanning && now - scanAt >= max(uint32_t(4000),sampleInterval)) {
            scanAt = now;
            scanning = WiFi.scanNetworks(true, true, true, 100) == WIFI_SCAN_RUNNING;
            if (!scanning) data.status = "SCAN ERROR";
        }
        if (scanning) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                foundCount = min(n, 32);
                for (size_t i = 0; i < foundCount; ++i) {
                    found[i] = Target();
                    WiFi.SSID(i).toCharArray(found[i].name, sizeof(found[i].name));
                    WiFi.BSSIDstr(i).toCharArray(found[i].address, sizeof(found[i].address));
                    const char *security="OTHER";
                    switch(WiFi.encryptionType(i)){case WIFI_AUTH_OPEN:security="OPEN";break;case WIFI_AUTH_WEP:security="WEP";break;case WIFI_AUTH_WPA_PSK:security="WPA";break;case WIFI_AUTH_WPA2_PSK:security="WPA2";break;case WIFI_AUTH_WPA_WPA2_PSK:security="WPA/WPA2";break;case WIFI_AUTH_WPA2_ENTERPRISE:security="WPA2 ENTERPRISE";break;case WIFI_AUTH_WPA3_PSK:security="WPA3";break;case WIFI_AUTH_WPA2_WPA3_PSK:security="WPA2/WPA3";break;default:break;}
                    strlcpy(found[i].detail,security,sizeof(found[i].detail));
                    found[i].rssi = WiFi.RSSI(i); found[i].channel = WiFi.channel(i);
                }
                ++metrics.events;
                for (int i = 0; i < n; ++i) {
                    if (!target.length() || target == WiFi.BSSIDstr(i)) {
                        ++metrics.rx; ++metrics.samples; metrics.rssi = WiFi.RSSI(i);
                    }
                }
                int sum = 0, strongest = -127, counts[14] = {}, added = 0, gone = 0;
                uint8_t macs[64][6] = {};
                int kept = min(n, 64);
                for (int i = 0; i < n; ++i) {
                    sum += WiFi.RSSI(i);
                    strongest = max(strongest, int(WiFi.RSSI(i)));
                    int ch = WiFi.channel(i);
                    if (ch >= 1 && ch <= 14) ++counts[ch - 1];
                    if (i < kept) {
                        memcpy(macs[i], WiFi.BSSID(i), 6);
                        bool known = false;
                        for (int j = 0; j < previousCount; ++j)
                            if (!memcmp(macs[i], previousMac[j], 6)) known = true;
                        if (!known) ++added;
                    }
                }
                for (int j = 0; j < previousCount; ++j) {
                    bool found = false;
                    for (int i = 0; i < kept; ++i)
                        if (!memcmp(macs[i], previousMac[j], 6)) found = true;
                    if (!found) ++gone;
                }
                if (haveScan && kept < 64 && previousCount < 64 && added + gone >= 8) {
                    churn = true;
                    churnAt = now;
                    event(WIFI, "SUSPICIOUS: AP churn (scan visibility)", true);
                }
                memcpy(previousMac, macs, sizeof(macs));
                previousCount = kept;
                haveScan = true;
                if(!target.length())metrics.rssi=n?sum/n:-127;
                int busiest = 0, occupied = 0;
                for (int i = 0; i < 14; ++i) {
                    if (counts[i]) ++occupied;
                    if (counts[i] > counts[busiest]) busiest = i;
                    data.bars[i] = min(100, counts[i] * 10);
                }
                data.lines[0] = "APs/BSSIDs: " + String(n) + " (nao SSIDs unicos)";
                data.lines[1] = "Canais: " + String(occupied) + " Busy CH " + String(busiest + 1);
                data.lines[2] =
                    n ? "RSSI avg/peak: " + String(sum / n) + " / " + String(strongest) : "RSSI: --";
                data.lines[3] = "AP novos/sumiram: " + String(added) + " / " + String(gone);
                WiFi.scanDelete();
                scanning = false;
            } else if (n == WIFI_SCAN_FAILED || now - scanAt > 6000) {
                esp_wifi_scan_stop();
                WiFi.scanDelete();
                scanning = false;
                data.status = "SCAN ERROR";
            }
        }
        if (now - window < 1000) return;
        if (shared && !owned) { window = now; return; }
        Traffic copy;
        portENTER_CRITICAL(&mux);
        copy = traffic;
        portEXIT_CRITICAL(&mux);
        uint32_t rate = perSecond(copy.packets - previous, now - window);
        uint32_t attacks = copy.deauth - previousDeauth;
        if (churn && now - churnAt > 5000) churn = false;
        static const char *labels[] = {"NORMAL", "BUSY", "HIGH TRAFFIC", "SUSPICIOUS", "DEAUTH DETECTED"};
        String previousStatus = data.status;
        data.status = labels[static_cast<unsigned>(wifiLevel(rate, baseline, attacks, churn))];
        uint8_t busiestPackets = 0;
        uint32_t highestPackets = 0;
        for (uint8_t ch = 0; ch < 14; ++ch) {
            uint32_t delta = copy.channels[ch] - previousChannels[ch];
            previousChannels[ch] = copy.channels[ch];
            if (delta > highestPackets) {
                highestPackets = delta;
                busiestPackets = ch + 1;
            }
        }
        data.lines[11] = "Packet hot CH: " + String(busiestPackets);
        data.warning = attacks || data.status == "SUSPICIOUS" || data.status == "HIGH TRAFFIC";
        if (data.warning || (data.status == "BUSY" && previousStatus != data.status))
            event(
                WIFI,
                data.status + " CH" + String(busiestPackets) + " / " + String(rate) + " pkt/s",
                data.warning
            );
        baseline = baseline == 0 ? rate : baseline * .8f + rate * .2f;
        previous = copy.packets;
        previousDeauth = copy.deauth;
        window = now;
        data.lines[4] = "Packets: " + String(copy.packets) + " /s: " + String(rate);
        data.lines[5] = "Management: " + String(copy.management);
        data.lines[6] = "Deauth/disassoc: " + String(copy.deauth);
        data.lines[7] = "TX MACs: " + String(copy.used) + (copy.used == 64 ? "+ (limite)" : "");
        data.lines[8] = "CH 1-14: barras = APs";
        data.lines[9] = "Amostragem por canal; nao total";
        data.lines[10] = "Alertas heuristicos, nao prova";
    }
    void end() override {
        if (!owned) {
            if (scanning) { esp_wifi_scan_stop(); WiFi.scanDelete(); scanning = false; }
            return;
        }
        portENTER_CRITICAL(&mux);
        listening = false;
        portEXIT_CRITICAL(&mux);
        if (hooked) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(nullptr);
        }
        if (scanning) esp_wifi_scan_stop();
        WiFi.scanDelete();
        WiFi.mode(WIFI_MODE_NULL);
        owned = false;
    }
};
} // namespace
std::unique_ptr<Monitor> makeWifi(bool shareConnection) { return std::unique_ptr<Monitor>(new WifiMonitor(shareConnection)); }
} // namespace CounterSuite
