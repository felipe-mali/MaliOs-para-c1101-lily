#include "core/bus_HAL.h"
#include "counter_main.h"
#include "modules/rfid/PN532.h"
#include <globals.h>
namespace CounterSuite {
namespace {
class NfcMonitor : public Monitor {
    std::unique_ptr<PN532> reader;
    uint32_t scans = 0, tags = 0, last = 0, at = 0, window = 0, burst = 0, interval = 0;
    uint8_t known[64][11] = {};
    uint8_t unique = 0;
    uint32_t lastSeen = 0;
    String lastUid;
    bool present = false, owned = false;

public:
    bool begin() override {
#if !defined(REMOVE_RFID_HW_INTERFACE)
        PN532::CONNECTION_TYPE connection;
        if (bruceConfigPins.rfidModule == PN532_I2C_MODULE) connection = PN532::I2C;
        else if (bruceConfigPins.rfidModule == PN532_SPI_MODULE) connection = PN532::SPI;
        else return false;
        reader.reset(new PN532(connection));
        owned = true;
        if (!reader->begin(true) || !reader->nfc.getFirmwareVersion()) return false;
        reader->nfc.setPassiveActivationRetries(0x00);
        data.status = "IDLE";
        window = millis();
        return true;
#else
        return false;
#endif
    }
    void tick(uint32_t now) override {
        if (now - at < max(uint32_t(250),sampleInterval)) return;
        at = now;
        ++scans;
        uint8_t uid[10] = {}, len = 0;
        uint32_t began = millis();
        bool detected = reader->readUidOnly(50) == RFIDInterface::SUCCESS;
        uint32_t elapsed = millis() - began;
        ++metrics.events; ++metrics.tx; ++metrics.samples;
        if (detected) { ++metrics.success; ++metrics.rx; } else ++metrics.failures;
        metrics.timeTotal += elapsed;
        metrics.timeMin = min(metrics.timeMin, elapsed); metrics.timeMax = max(metrics.timeMax, elapsed);
        if (detected) {
            len = reader->uid.size;
            memcpy(uid, reader->uid.uidByte, len);
        }
        if (detected && len > 0 && len <= 10) {
            String text;
            for (int i = 0; i < len; ++i) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", uid[i]);
                text += hex;
            }
            if (!present || text != lastUid) {
                interval = tags ? now - last : 0;
                last = now;
                ++tags;
                ++burst;
                bool seen = false;
                for (int i = 0; i < unique; ++i)
                    if (known[i][0] == len && !memcmp(known[i] + 1, uid, len)) seen = true;
                if (!seen && unique < 64) {
                    known[unique][0] = len;
                    memcpy(known[unique] + 1, uid, len);
                    ++unique;
                }
                event(NFC, "ISO14443A tag detected");
            }
            lastUid = text;
            present = true;
            lastSeen = now;
            data.status = "SIGNAL";
        } else {
            if (now - lastSeen > 750) present = false;
            data.status = present ? "SIGNAL" : "IDLE";
        }
        if (now - window >= 3000) {
            window = now;
            burst = 0;
        }
        if (burst >= 5) {
            if (!data.warning) event(NFC, "RAPID NFC ACTIVITY", true);
            data.status = "RAPID NFC ACTIVITY";
        }
        data.warning = burst >= 5;
        data.lines[0] = "Scans: " + String(scans) + " Tags: " + String(tags);
        data.lines[1] = "Unique: " + String(unique) + (unique == 64 ? "+ (limite)" : "");
        data.lines[2] = "Last type: " + String(tags ? reader->printableUID.picc_type : "--");
        data.lines[3] = "UID: " + lastUid;
        data.lines[4] = "Interval: " + String(interval) + " ms";
        data.lines[5] = tags ? "Last: up " + String(last / 1000) + " s" : "Last: --";
        data.lines[6] = "PN532: consulta UID, sem escrita";
        data.lines[7] = "Tag parada conta uma presenca";
    }
    void end() override {
        if (owned) {
            if (reader) reader->nfc.powerDown();
            reader.reset();
            releaseI2CBus();
            owned = false;
        }
    }
};
} // namespace
std::unique_ptr<Monitor> makeNfc() { return std::unique_ptr<Monitor>(new NfcMonitor); }
} // namespace CounterSuite
