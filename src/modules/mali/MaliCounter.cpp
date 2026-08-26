#include "MaliCounter.h"

#include "core/display.h"
#include "core/radio_mem.h"
#include "modules/NRF24/nrf_common.h"
#include "modules/rf/rf_utils.h"
#include <globals.h>

#include <WiFi.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <soc/soc_caps.h>

#if defined(USE_W5500_VIA_SPI) && !defined(LITE_VERSION)
#include <ETH.h>
#endif

#if SOC_BLE_SUPPORTED
#include <NimBLEDevice.h>
#endif

namespace {
constexpr uint32_t RENDER_INTERVAL_MS = 150;
constexpr uint32_t BLE_SCAN_MS = 900;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 5000;
constexpr uint32_t BLE_SCAN_TIMEOUT_MS = 3000;
constexpr uint32_t RADIO_SETTLE_MS = 120;
constexpr uint8_t NRF_CHANNELS = 80;
constexpr int SUB_RSSI_THRESHOLD = -75;
constexpr uint32_t RESET_CONTEXT_MAGIC = 0x4D414C49; // "MALI"

const float SUB_FREQUENCIES[] = {
    300.00f,
    315.00f,
    390.00f,
    433.92f,
    434.42f,
    868.35f,
    915.00f,
};

enum class Phase {
    WIFI_START,
    WIFI_WAIT,
    BLE_START,
    BLE_WAIT,
    NRF_START,
    NRF_SWEEP,
    CC_START,
    CC_SWEEP,
    SETTLE,
    PAUSE,
};

struct MaliResetContext {
    uint32_t magic;
    uint32_t active;
    uint32_t cycle;
    uint32_t phase;
    uint32_t freeHeap;
    uint32_t largestDma;
};

RTC_NOINIT_ATTR MaliResetContext resetContext;

struct CounterStats {
    uint16_t wifiNetworks = 0;
    int32_t wifiRssiSum = 0;
    int16_t wifiStrongest = -127;
    uint16_t wifiChannels = 0;

    uint32_t bleAdvertisements = 0;
    int32_t bleRssiSum = 0;
    uint16_t bleUnique = 0;
    int16_t bleStrongest = -127;
    uint32_t bleHashes[32] = {0};

    uint16_t nrfEvents = 0;
    uint8_t nrfBusiest = 0;
    uint8_t nrfLevels[NRF_CHANNELS] = {0};

    uint16_t subPeaks = 0;
    int16_t subStrongest = -127;
    float subStrongestFrequency = 0.0f;
};

portMUX_TYPE bleMux = portMUX_INITIALIZER_UNLOCKED;

uint32_t hashAddress(const std::string &address) {
    uint32_t hash = 2166136261UL;
    for (char value : address) {
        hash ^= static_cast<uint8_t>(value);
        hash *= 16777619UL;
    }
    return hash;
}

#if SOC_BLE_SUPPORTED
class PassiveBleCallbacks : public NimBLEScanCallbacks {
public:
    void bind(CounterStats *stats) { _stats = stats; }

    void onResult(const NimBLEAdvertisedDevice *device) override {
        if (_stats == nullptr || device == nullptr) return;

        const int16_t rssi = device->getRSSI();
        const uint32_t addressHash = hashAddress(device->getAddress().toString());

        portENTER_CRITICAL(&bleMux);
        _stats->bleAdvertisements++;
        _stats->bleRssiSum += rssi;
        if (rssi > _stats->bleStrongest) _stats->bleStrongest = rssi;

        bool known = false;
        for (uint16_t i = 0; i < _stats->bleUnique; ++i) {
            if (_stats->bleHashes[i] == addressHash) {
                known = true;
                break;
            }
        }
        if (!known && _stats->bleUnique < 32) {
            _stats->bleHashes[_stats->bleUnique] = addressHash;
            _stats->bleUnique++;
        }
        portEXIT_CRITICAL(&bleMux);
    }

private:
    CounterStats *_stats = nullptr;
};

PassiveBleCallbacks bleCallbacks;
#endif

const char *phaseName(Phase phase) {
    switch (phase) {
        case Phase::WIFI_START:
        case Phase::WIFI_WAIT: return "Wi-Fi passivo";
        case Phase::BLE_START:
        case Phase::BLE_WAIT: return "BLE passivo";
        case Phase::NRF_START:
        case Phase::NRF_SWEEP: return "nRF24 em recepcao";
        case Phase::CC_START:
        case Phase::CC_SWEEP: return "CC1101 em recepcao";
        case Phase::SETTLE: return "Estabilizando radios";
        case Phase::PAUSE: return "Ciclo concluido";
    }
    return "Aguardando";
}

int32_t average(int32_t total, uint32_t count) { return count == 0 ? 0 : total / static_cast<int32_t>(count); }

const char *resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        case ESP_RST_USB: return "USB";
        case ESP_RST_JTAG: return "JTAG";
        case ESP_RST_EFUSE: return "EFUSE";
        case ESP_RST_PWR_GLITCH: return "POWER_GLITCH";
        case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
    }
    return "INVALID";
}

class PassiveCounterSession {
public:
    void run() {
        _initialWifiMode = WiFi.getMode();
        beginCycle();
        logState("session-start");
        draw(true);

        delay(120);
        check(EscPress);
        check(SelPress);

        while (!_failed && !check(EscPress)) {
            if (check(NextPress) || check(SelPress)) {
                _page = (_page + 1) % 5;
                draw(true);
            }
            if (check(PrevPress)) {
                _page = (_page + 4) % 5;
                draw(true);
            }

            step();

            if (millis() - _lastRender >= RENDER_INTERVAL_MS) draw(false);
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        cleanup();
        resetContext.active = 0;
        Serial.printf(
            "[MALI][Counter] session-end cycles=%lu failed=%u\n",
            static_cast<unsigned long>(_cycles),
            _failed
        );
        Serial.flush();
        if (_failed) displayError(_failure, true);
    }

private:
    CounterStats _stats;
    Phase _phase = Phase::WIFI_START;
    Phase _nextPhase = Phase::WIFI_START;
    uint8_t _page = 0;
    uint8_t _nrfChannel = 0;
    uint8_t _subIndex = 0;
    uint32_t _pauseStarted = 0;
    uint32_t _phaseStarted = 0;
    uint32_t _settleStarted = 0;
    uint32_t _lastRender = 0;
    uint32_t _cycles = 0;
    String _failure;
    wifi_mode_t _initialWifiMode = WIFI_MODE_NULL;
    bool _wifiScanStarted = false;
    bool _wifiReady = false;
    bool _wifiModeChanged = false;
    bool _bleOwned = false;
    bool _nrfOwned = false;
    bool _nrfPinsClaimed = false;
    bool _ccOwned = false;
    bool _ccPinsClaimed = false;
    bool _failed = false;
#if SOC_BLE_SUPPORTED
    NimBLEScan *_bleScan = nullptr;
#endif

    bool spiReady(const BruceConfigPins::SPIPins &pins) const {
        return pins.sck != GPIO_NUM_NC && pins.miso != GPIO_NUM_NC && pins.mosi != GPIO_NUM_NC &&
               pins.cs != GPIO_NUM_NC && pins.io0 != GPIO_NUM_NC;
    }

    void rememberContext() const {
        resetContext.magic = RESET_CONTEXT_MAGIC;
        resetContext.active = 1;
        resetContext.cycle = _cycles;
        resetContext.phase = static_cast<uint32_t>(_phase);
        resetContext.freeHeap = ESP.getFreeHeap();
        resetContext.largestDma = radioLargestDmaBlock();
    }

    void logState(const char *event) const {
        rememberContext();
        const bool heapOk = heap_caps_check_integrity_all(false);
        Serial.printf(
            "[MALI][Counter] cycle=%lu phase=%s event=%s heap=%u min=%u internal=%u largest=%u "
            "dma=%u stack_hwm=%u heap_ok=%u\n",
            static_cast<unsigned long>(_cycles),
            phaseName(_phase),
            event,
            static_cast<unsigned>(ESP.getFreeHeap()),
            static_cast<unsigned>(ESP.getMinFreeHeap()),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(radioLargestDmaBlock()),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
            heapOk
        );
        Serial.flush();
    }

    void fail(const String &message) {
        if (_failed) return;
        _failure = message;
        _failed = true;
        logState("FAIL");
        Serial.printf("[MALI][Counter][ERROR] %s\n", message.c_str());
        Serial.flush();
    }

    bool heapHealthyBefore(const char *subsystem) {
        if (heap_caps_check_integrity_all(true)) return true;
        fail(String("Heap inconsistente antes de ") + subsystem);
        return false;
    }

    void setPhase(Phase phase) {
        _phase = phase;
        _phaseStarted = millis();
        logState("enter");
    }

    void settleBefore(Phase nextPhase) {
        _nextPhase = nextPhase;
        _settleStarted = millis();
        setPhase(Phase::SETTLE);
    }

    void beginCycle() {
        _stats = CounterStats();
        _nrfChannel = 0;
        _subIndex = 0;
        setPhase(Phase::WIFI_START);
    }

    void step() {
        switch (_phase) {
            case Phase::WIFI_START: startWifi(); break;
            case Phase::WIFI_WAIT: pollWifi(); break;
            case Phase::BLE_START: startBle(); break;
            case Phase::BLE_WAIT: pollBle(); break;
            case Phase::NRF_START: startNrf(); break;
            case Phase::NRF_SWEEP: sweepNrf(); break;
            case Phase::CC_START: startCc(); break;
            case Phase::CC_SWEEP: sweepCc(); break;
            case Phase::SETTLE:
                if (millis() - _settleStarted >= RADIO_SETTLE_MS) setPhase(_nextPhase);
                break;
            case Phase::PAUSE:
                if (millis() - _pauseStarted >= 750) {
                    _cycles++;
                    beginCycle();
                }
                break;
        }
    }

    void startWifi() {
#if SOC_WIFI_SUPPORTED
        if (!heapHealthyBefore("Wi-Fi")) return;

        if (!_wifiReady) {
            const bool staAlreadyEnabled = (_initialWifiMode & WIFI_MODE_STA) != 0;
            if (!staAlreadyEnabled) {
                if (!radioHasMemForWifi()) {
                    fail("Pouca RAM contigua para Wi-Fi");
                    return;
                }

                const wifi_mode_t scanMode = static_cast<wifi_mode_t>(_initialWifiMode | WIFI_MODE_STA);
                if (!WiFi.mode(scanMode)) {
                    fail("Falha ao iniciar Wi-Fi");
                    return;
                }
                _wifiModeChanged = true;
            }
            _wifiReady = true;
            logState("wifi-ready");
        }

        WiFi.scanDelete();
        const int16_t result = WiFi.scanNetworks(true, true, true, 80);
        _wifiScanStarted = result == WIFI_SCAN_RUNNING;
        if (_wifiScanStarted) {
            setPhase(Phase::WIFI_WAIT);
        } else if (result >= 0) {
            collectWifiResults(result);
        } else {
            fail("Falha ao iniciar busca Wi-Fi");
        }
#else
        settleBefore(Phase::BLE_START);
#endif
    }

    void pollWifi() {
#if SOC_WIFI_SUPPORTED
        if (millis() - _phaseStarted > WIFI_SCAN_TIMEOUT_MS) {
            esp_wifi_scan_stop();
            WiFi.scanDelete();
            _wifiScanStarted = false;
            fail("Timeout na busca Wi-Fi");
            return;
        }

        const int16_t result = WiFi.scanComplete();
        if (result == WIFI_SCAN_RUNNING) return;
        if (result == WIFI_SCAN_FAILED) {
            _wifiScanStarted = false;
            WiFi.scanDelete();
            fail("Falha durante busca Wi-Fi");
            return;
        }

        collectWifiResults(result);
#else
        settleBefore(Phase::BLE_START);
#endif
    }

    void collectWifiResults(int16_t result) {
#if SOC_WIFI_SUPPORTED
        if (result > 0) {
            uint16_t channelMask = 0;
            for (int16_t i = 0; i < result; ++i) {
                int16_t rssi = WiFi.RSSI(i);
                int32_t channel = WiFi.channel(i);
                _stats.wifiRssiSum += rssi;
                if (rssi > _stats.wifiStrongest) _stats.wifiStrongest = rssi;
                if (channel >= 1 && channel <= 14) channelMask |= static_cast<uint16_t>(1U << (channel - 1));
            }
            _stats.wifiNetworks = result;
            for (uint8_t channel = 0; channel < 14; ++channel) {
                if (channelMask & (1U << channel)) _stats.wifiChannels++;
            }
        }

        WiFi.scanDelete();
        _wifiScanStarted = false;
        logState("wifi-scan-complete");
#endif
        settleBefore(Phase::BLE_START);
    }

    void finishWifi() {
#if SOC_WIFI_SUPPORTED
        if (_wifiScanStarted) {
            const esp_err_t stopResult = esp_wifi_scan_stop();
            Serial.printf("[MALI][Counter] wifi-scan-stop result=%s\n", esp_err_to_name(stopResult));
        }
        WiFi.scanDelete();
        _wifiScanStarted = false;
        if (_wifiModeChanged) {
            const bool restored = WiFi.mode(_initialWifiMode);
            Serial.printf(
                "[MALI][Counter] wifi-restore mode=%d result=%u\n",
                static_cast<int>(_initialWifiMode),
                restored
            );
        }
        _wifiReady = false;
        _wifiModeChanged = false;
#endif
    }

    void startBle() {
#if SOC_BLE_SUPPORTED
        if (!heapHealthyBefore("BLE")) return;

        if (!_bleOwned) {
            if (NimBLEDevice::isInitialized()) {
                fail("BLE ja esta em uso por outra funcao");
                return;
            }
            if (radioLargestDmaBlock() < RADIO_BLE_MIN_DMA_BLOCK) {
                fail("Pouca RAM contigua para BLE");
                return;
            }
            if (!NimBLEDevice::init("")) {
                fail("Falha ao iniciar BLE");
                return;
            }

            _bleOwned = true;
            _bleScan = NimBLEDevice::getScan();
            if (_bleScan == nullptr) {
                fail("Falha ao obter scanner BLE");
                return;
            }

            _bleScan->setActiveScan(false);
            _bleScan->setInterval(80);
            _bleScan->setWindow(40);
            _bleScan->setDuplicateFilter(false);
            _bleScan->setMaxResults(32);
            logState("ble-ready");
        }

        if (_bleScan == nullptr) {
            fail("Scanner BLE indisponivel");
            return;
        }
        bleCallbacks.bind(&_stats);
        _bleScan->clearResults();
        _bleScan->setScanCallbacks(&bleCallbacks, true);

        if (_bleScan->start(BLE_SCAN_MS, false, true)) {
            setPhase(Phase::BLE_WAIT);
        } else {
            bleCallbacks.bind(nullptr);
            _bleScan->setScanCallbacks(nullptr);
            fail("Falha ao iniciar busca BLE");
        }
#else
        settleBefore(Phase::NRF_START);
#endif
    }

    void pollBle() {
#if SOC_BLE_SUPPORTED
        if (millis() - _phaseStarted > BLE_SCAN_TIMEOUT_MS) {
            finishBleScan();
            fail("Timeout na busca BLE");
            return;
        }
        if (_bleScan != nullptr && _bleScan->isScanning()) return;
        if (!finishBleScan()) {
            fail("Falha ao encerrar busca BLE");
            return;
        }
        logState("ble-scan-complete");
#endif
        settleBefore(Phase::NRF_START);
    }

    bool finishBleScan() {
#if SOC_BLE_SUPPORTED
        if (_bleScan != nullptr) {
            bleCallbacks.bind(nullptr);
            _bleScan->setScanCallbacks(nullptr);
            if (_bleScan->isScanning()) {
                const bool stopRequested = _bleScan->stop();
                const uint32_t stopStarted = millis();
                while (_bleScan->isScanning() && millis() - stopStarted < 1200) delay(10);
                if (!stopRequested || _bleScan->isScanning()) {
                    Serial.println("[MALI][Counter][ERROR] BLE scan did not stop");
                    return false;
                }
            }
            _bleScan->clearResults();
        }
#endif
        return true;
    }

    void finishBle() {
#if SOC_BLE_SUPPORTED
        const bool scanStopped = finishBleScan();
        if (_bleOwned && scanStopped) {
            delay(RADIO_SETTLE_MS);
            const bool deinitialized = NimBLEDevice::deinit(true);
            Serial.printf("[MALI][Counter] ble-deinit result=%u\n", deinitialized);
            if (!deinitialized && !_failed) fail("Falha ao encerrar BLE");
        } else if (_bleOwned) {
            Serial.println("[MALI][Counter][ERROR] BLE deinit skipped: scan still active");
            if (!_failed) fail("Falha ao parar BLE na saida");
        }
        _bleScan = nullptr;
        _bleOwned = false;
#endif
    }

    void startNrf() {
#if defined(USE_NRF24_VIA_SPI)
        if (!heapHealthyBefore("nRF24")) return;
        if (!spiReady(bruceConfigPins.NRF24_bus)) {
            fail("Pinos nRF24 nao configurados");
            return;
        }
        if (gpsConnected) {
            fail("nRF24 indisponivel: GPIO 43/44 usados pelo GPS");
            return;
        }
#if defined(USE_W5500_VIA_SPI) && !defined(LITE_VERSION)
        if (ETH.started()) {
            fail("nRF24 indisponivel: GPIO 43/44 usados pelo W5500");
            return;
        }
#endif

        _nrfPinsClaimed = true;
        if (!nrf_start(NRF_MODE_SPI) || !NRFradio.isChipConnected()) {
            finishNrf();
            fail("nRF24 nao encontrado");
            return;
        }

        _nrfOwned = true;
        digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
        NRFradio.setAutoAck(false);
        NRFradio.disableCRC();
        NRFradio.setAddressWidth(2);
        const uint8_t noiseAddress[][2] = {
            {0x55, 0x55}, {0xAA, 0xAA}, {0xA0, 0xAA},
            {0xAB, 0xAA}, {0xAC, 0xAA}, {0xAD, 0xAA},
        };
        for (uint8_t i = 0; i < 6; ++i) NRFradio.openReadingPipe(i, noiseAddress[i]);
        NRFradio.setDataRate(RF24_1MBPS);
        logState("nrf-ready");
        setPhase(Phase::NRF_SWEEP);
#else
        settleBefore(Phase::CC_START);
#endif
    }

    void sweepNrf() {
#if defined(USE_NRF24_VIA_SPI)
        for (uint8_t slice = 0; slice < 4 && _nrfChannel < NRF_CHANNELS; ++slice, ++_nrfChannel) {
            NRFradio.setChannel(_nrfChannel);
            NRFradio.startListening();
            delayMicroseconds(128);
            NRFradio.stopListening();

            if (NRFradio.testRPD()) {
                _stats.nrfEvents++;
                if (_stats.nrfLevels[_nrfChannel] < 255) _stats.nrfLevels[_nrfChannel]++;
                if (_stats.nrfLevels[_nrfChannel] > _stats.nrfLevels[_stats.nrfBusiest]) {
                    _stats.nrfBusiest = _nrfChannel;
                }
            }
        }

        if (_nrfChannel >= NRF_CHANNELS) {
            finishNrf();
            logState("nrf-sweep-complete");
            settleBefore(Phase::CC_START);
        }
#else
        settleBefore(Phase::CC_START);
#endif
    }

    void finishNrf() {
#if defined(USE_NRF24_VIA_SPI)
        if (_nrfOwned) {
            NRFradio.stopListening();
            NRFradio.powerDown();
        }
        if (_nrfPinsClaimed && bruceConfigPins.NRF24_bus.io0 != GPIO_NUM_NC) {
            pinMode(bruceConfigPins.NRF24_bus.io0, OUTPUT);
            digitalWrite(bruceConfigPins.NRF24_bus.io0, LOW);
        }
        if (_nrfPinsClaimed && bruceConfigPins.NRF24_bus.cs != GPIO_NUM_NC) {
            pinMode(bruceConfigPins.NRF24_bus.cs, OUTPUT);
            digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
        }
        _nrfOwned = false;
        _nrfPinsClaimed = false;
#endif
    }

    void startCc() {
#if defined(USE_CC1101_VIA_SPI)
        if (!heapHealthyBefore("CC1101")) return;
        if (bruceConfigPins.rfModule != CC1101_SPI_MODULE || !spiReady(bruceConfigPins.CC1101_bus)) {
            fail("CC1101 nao configurado");
            return;
        }

        _ccPinsClaimed = true;
        if (!initRfModule("rx", SUB_FREQUENCIES[0])) {
            finishCc();
            fail("CC1101 nao encontrado");
            return;
        }

        _ccOwned = true;
        logState("cc1101-ready");
        setPhase(Phase::CC_SWEEP);
#else
        beginPause();
#endif
    }

    void sweepCc() {
#if defined(USE_CC1101_VIA_SPI)
        if (_subIndex < sizeof(SUB_FREQUENCIES) / sizeof(SUB_FREQUENCIES[0])) {
            const float frequency = SUB_FREQUENCIES[_subIndex++];
            setMHZ(frequency);
            delay(2);
            const int16_t rssi = ELECHOUSE_cc1101.getRssi();
            if (rssi > _stats.subStrongest) {
                _stats.subStrongest = rssi;
                _stats.subStrongestFrequency = frequency;
            }
            if (rssi >= SUB_RSSI_THRESHOLD) _stats.subPeaks++;
        }

        if (_subIndex >= sizeof(SUB_FREQUENCIES) / sizeof(SUB_FREQUENCIES[0])) {
            finishCc();
            logState("cc1101-sweep-complete");
            beginPause();
        }
#else
        beginPause();
#endif
    }

    void finishCc() {
#if defined(USE_CC1101_VIA_SPI)
        if (_ccOwned) deinitRfModule();
        if (_ccPinsClaimed && bruceConfigPins.CC1101_bus.cs != GPIO_NUM_NC) {
            pinMode(bruceConfigPins.CC1101_bus.cs, OUTPUT);
            digitalWrite(bruceConfigPins.CC1101_bus.cs, HIGH);
        }
        _ccOwned = false;
        _ccPinsClaimed = false;
#endif
    }

    void beginPause() {
        _pauseStarted = millis();
        setPhase(Phase::PAUSE);
    }

    void cleanup() {
        logState("cleanup-start");
        finishCc();
        finishNrf();
        finishBle();
        finishWifi();
        logState("cleanup-end");
    }

    void printLine(int16_t y, const String &text, uint16_t color = 0) {
        tft.setTextColor(color == 0 ? bruceConfig.priColor : color, bruceConfig.bgColor);
        tft.drawString(text, 8, y, 1);
    }

    void draw(bool force) {
        if (!force && millis() - _lastRender < RENDER_INTERVAL_MS) return;
        _lastRender = millis();

        drawMainBorderWithTitle(_page == 0 ? "Mali Counter" : pageTitle());
        tft.fillRect(5, 24, tftWidth - 10, tftHeight - 43, bruceConfig.bgColor);
        tft.setTextSize(FP);

        if (_page == 0) drawDashboard();
        else if (_page == 1) drawWifi();
        else if (_page == 2) drawBle();
        else if (_page == 3) drawNrf();
        else drawSub();

        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawCentreString("NEXT/PREV: tela  ESC: sair", tftWidth / 2, tftHeight - 16, 1);
    }

    const char *pageTitle() const {
        switch (_page) {
            case 1: return "Detalhes Wi-Fi";
            case 2: return "Detalhes BLE";
            case 3: return "Atividade 2.4 GHz";
            case 4: return "Atividade Sub-GHz";
            default: return "Mali Counter";
        }
    }

    void drawDashboard() {
        uint32_t bleAds;
        uint16_t bleUnique;
        portENTER_CRITICAL(&bleMux);
        bleAds = _stats.bleAdvertisements;
        bleUnique = _stats.bleUnique;
        portEXIT_CRITICAL(&bleMux);

        const uint32_t total = _stats.wifiNetworks + bleUnique + _stats.nrfEvents + _stats.subPeaks;
        printLine(27, "Wi-Fi: " + String(_stats.wifiNetworks) + " redes");
        printLine(45, "BLE: " + String(bleAds) + " anuncios / " + String(bleUnique) + " unicos");
        printLine(63, "2.4 GHz: " + String(_stats.nrfEvents) + " eventos RX");
        printLine(81, "Sub-GHz: " + String(_stats.subPeaks) + " picos RX");
        printLine(99, "Atividade total: " + String(total), bruceConfig.secColor);
        printLine(117, String("Fase: ") + phaseName(_phase));
        printLine(135, "Ciclos: " + String(_cycles));
    }

    void drawWifi() {
        printLine(30, "Redes unicas: " + String(_stats.wifiNetworks));
        printLine(52, "Canais ocupados: " + String(_stats.wifiChannels));
        printLine(74, "RSSI medio: " + String(average(_stats.wifiRssiSum, _stats.wifiNetworks)) + " dBm");
        printLine(96, "Mais forte: " + String(_stats.wifiStrongest) + " dBm");
        printLine(122, "Busca passiva; sem probe request", bruceConfig.secColor);
    }

    void drawBle() {
        uint32_t ads;
        uint16_t unique;
        int32_t rssiSum;
        int16_t strongest;
        portENTER_CRITICAL(&bleMux);
        ads = _stats.bleAdvertisements;
        unique = _stats.bleUnique;
        rssiSum = _stats.bleRssiSum;
        strongest = _stats.bleStrongest;
        portEXIT_CRITICAL(&bleMux);

        printLine(30, "Anuncios recebidos: " + String(ads));
        printLine(52, "Dispositivos unicos: " + String(unique));
        printLine(74, "RSSI medio: " + String(average(rssiSum, ads)) + " dBm");
        printLine(96, "Mais forte: " + String(strongest) + " dBm");
        printLine(122, "Busca passiva; sem conexao", bruceConfig.secColor);
    }

    void drawNrf() {
        printLine(30, "Eventos RPD: " + String(_stats.nrfEvents));
        printLine(52, "Canal mais ativo: " + String(_stats.nrfBusiest));
        printLine(74, "Canal atual: " + String(_nrfChannel));
        printLine(96, "Faixa: 2.400 a 2.479 GHz");
        printLine(122, "Somente recepcao; CE baixo ao sair", bruceConfig.secColor);
    }

    void drawSub() {
        printLine(30, "Picos acima de -75 dBm: " + String(_stats.subPeaks));
        printLine(52, "RSSI mais forte: " + String(_stats.subStrongest) + " dBm");
        printLine(74, "Frequencia: " + String(_stats.subStrongestFrequency, 2) + " MHz");
        printLine(96, "Amostras: " + String(_subIndex));
        printLine(122, "Somente recepcao; modulo em IDLE", bruceConfig.secColor);
    }
};
} // namespace

namespace MaliCounter {
void printBootDiagnostics() {
    const esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("[RESET] reason=%d name=%s\n", static_cast<int>(reason), resetReasonName(reason));

    if (resetContext.magic == RESET_CONTEXT_MAGIC && resetContext.active == 1 &&
        resetContext.phase <= static_cast<uint32_t>(Phase::PAUSE)) {
        const Phase previousPhase = static_cast<Phase>(resetContext.phase);
        Serial.printf(
            "[MALI][Counter][PREVIOUS_RESET] cycle=%lu phase=%s heap=%lu dma=%lu\n",
            static_cast<unsigned long>(resetContext.cycle),
            phaseName(previousPhase),
            static_cast<unsigned long>(resetContext.freeHeap),
            static_cast<unsigned long>(resetContext.largestDma)
        );
    }
    resetContext.active = 0;
    Serial.flush();
}

void run() { PassiveCounterSession().run(); }
}
