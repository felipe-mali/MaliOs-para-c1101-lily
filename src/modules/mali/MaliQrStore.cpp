#include "MaliQrStore.h"

#include "MaliQrService.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <globals.h>
#include <vector>

namespace {

constexpr char kStoreDirectory[] = "/MaliOS";
constexpr char kHistoryPath[] = "/MaliOS/qr_history.json";
constexpr char kHistoryTempPath[] = "/MaliOS/qr_history.tmp";
constexpr char kHistoryBackupPath[] = "/MaliOS/qr_history.bak";
constexpr uint8_t kFormatVersion = 1;
constexpr size_t kMaxLabelBytes = 48;

StaticSemaphore_t storeMutexControl;
SemaphoreHandle_t storeMutex = nullptr;
portMUX_TYPE storeInitMux = portMUX_INITIALIZER_UNLOCKED;

fs::FS *historyFs = nullptr;
std::vector<MaliQrStore::HistoryEntry> historyEntries;
size_t configuredHistoryLimit = MaliQrStore::kDefaultHistoryLimit;
uint32_t dirtySince = 0;
bool initialized = false;
bool dirty = false;

class StoreLock {
public:
    StoreLock() : locked(storeMutex && xSemaphoreTake(storeMutex, portMAX_DELAY) == pdTRUE) {}
    ~StoreLock() {
        if (locked) xSemaphoreGive(storeMutex);
    }
    explicit operator bool() const { return locked; }

private:
    bool locked;
};

void ensureMutex() {
    if (storeMutex) return;

    portENTER_CRITICAL(&storeInitMux);
    if (!storeMutex) storeMutex = xSemaphoreCreateMutexStatic(&storeMutexControl);
    portEXIT_CRITICAL(&storeInitMux);
}

size_t clampHistoryLimit(size_t limit) {
    if (limit < 1) return 1;
    if (limit > MaliQrStore::kMaxHistoryLimit) return MaliQrStore::kMaxHistoryLimit;
    return limit;
}

void setError(String *errorOut, const __FlashStringHelper *message) {
    if (errorOut) *errorOut = message;
}

bool validateLabel(const String &label, bool allowEmpty, String *errorOut) {
    if (label.isEmpty()) {
        if (allowEmpty) return true;
        setError(errorOut, F("Nome obrigatorio"));
        return false;
    }
    if (label.length() > kMaxLabelBytes) {
        setError(errorOut, F("Nome excede 48 bytes"));
        return false;
    }

    String validationError;
    if (!MaliQrService::validatePayload(label, validationError)) {
        if (errorOut) *errorOut = validationError;
        return false;
    }
    return true;
}

bool validateStoredPayload(const String &payload, String *errorOut) {
    String validationError;
    if (!MaliQrService::validatePayload(payload, validationError)) {
        if (errorOut) *errorOut = validationError;
        return false;
    }
    return true;
}

void markDirtyLocked() {
    if (!dirty) dirtySince = millis();
    dirty = true;
}

void trimHistoryLocked() {
    if (historyEntries.size() > configuredHistoryLimit) {
        historyEntries.resize(configuredHistoryLimit);
    }
}

bool recoverInterruptedWriteLocked() {
    if (!historyFs || historyFs->exists(kHistoryPath)) return true;

    if (!historyFs->exists(kHistoryTempPath)) {
        if (!historyFs->exists(kHistoryBackupPath)) return true;
        if (!historyFs->rename(kHistoryBackupPath, kHistoryPath)) {
            Serial.println(F("[MaliQrStore] Falha ao recuperar backup do historico"));
            return false;
        }
        Serial.println(F("[MaliQrStore] Backup do historico recuperado"));
        return true;
    }

    if (!historyFs->rename(kHistoryTempPath, kHistoryPath)) {
        Serial.println(F("[MaliQrStore] Falha ao recuperar historico temporario"));
        return false;
    }
    Serial.println(F("[MaliQrStore] Historico recuperado apos escrita interrompida"));
    return true;
}

bool loadHistoryLocked() {
    historyEntries.clear();
    configuredHistoryLimit = MaliQrStore::kDefaultHistoryLimit;
    dirty = false;
    dirtySince = 0;

    if (!historyFs) return false;
    recoverInterruptedWriteLocked();
    if (!historyFs->exists(kHistoryPath)) return true;

    File file = historyFs->open(kHistoryPath, FILE_READ);
    if (!file) {
        Serial.println(F("[MaliQrStore] Nao foi possivel abrir o historico"));
        return false;
    }

    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, file);
    file.close();
    if (parseError) {
        Serial.printf("[MaliQrStore] Historico invalido: %s\n", parseError.c_str());
        return false;
    }

    const int storedLimit = document["limit"] | static_cast<int>(MaliQrStore::kDefaultHistoryLimit);
    configuredHistoryLimit = clampHistoryLimit(storedLimit > 0 ? static_cast<size_t>(storedLimit) : 1);
    historyEntries.reserve(configuredHistoryLimit);

    JsonArrayConst items = document["items"].as<JsonArrayConst>();
    for (JsonObjectConst item : items) {
        const String payload = item["payload"] | "";
        const String label = item["label"] | "";
        if (!validateStoredPayload(payload, nullptr) || !validateLabel(label, true, nullptr)) continue;
        if (!historyEntries.empty() && historyEntries.back().payload == payload) continue;

        historyEntries.push_back({label, payload});
        if (historyEntries.size() >= configuredHistoryLimit) break;
    }

    Serial.printf(
        "[MaliQrStore] Historico carregado (%u item(ns), limite %u)\n",
        static_cast<unsigned>(historyEntries.size()),
        static_cast<unsigned>(configuredHistoryLimit)
    );
    return true;
}

bool ensureInitializedLocked() {
    if (initialized) return historyFs != nullptr;

    historyFs = &LittleFS;
    initialized = true;
    return loadHistoryLocked();
}

bool replaceHistoryFileLocked() {
    const bool hadPreviousFile = historyFs->exists(kHistoryPath);
    if (historyFs->exists(kHistoryBackupPath)) historyFs->remove(kHistoryBackupPath);

    if (hadPreviousFile && !historyFs->rename(kHistoryPath, kHistoryBackupPath)) {
        Serial.println(F("[MaliQrStore] Falha ao proteger historico anterior"));
        return false;
    }

    if (!historyFs->rename(kHistoryTempPath, kHistoryPath)) {
        Serial.println(F("[MaliQrStore] Falha ao ativar novo historico"));
        if (hadPreviousFile) historyFs->rename(kHistoryBackupPath, kHistoryPath);
        return false;
    }

    if (hadPreviousFile) historyFs->remove(kHistoryBackupPath);
    return true;
}

bool flushLocked() {
    if (!dirty) return true;
    if (!historyFs) return false;

    if (!historyFs->exists(kStoreDirectory) && !historyFs->mkdir(kStoreDirectory)) {
        Serial.println(F("[MaliQrStore] Falha ao criar diretorio do historico"));
        return false;
    }

    if (historyFs->exists(kHistoryTempPath)) historyFs->remove(kHistoryTempPath);
    File file = historyFs->open(kHistoryTempPath, FILE_WRITE);
    if (!file) {
        Serial.println(F("[MaliQrStore] Falha ao criar historico temporario"));
        return false;
    }

    JsonDocument document;
    document["version"] = kFormatVersion;
    document["limit"] = configuredHistoryLimit;
    JsonArray items = document["items"].to<JsonArray>();
    for (const auto &entry : historyEntries) {
        JsonObject item = items.add<JsonObject>();
        item["label"] = entry.label;
        item["payload"] = entry.payload;
    }

    const size_t bytesWritten = serializeJson(document, file);
    file.flush();
    file.close();
    if (bytesWritten == 0) {
        historyFs->remove(kHistoryTempPath);
        Serial.println(F("[MaliQrStore] Falha ao serializar historico"));
        return false;
    }

    if (!replaceHistoryFileLocked()) return false;

    dirty = false;
    dirtySince = 0;
    Serial.printf(
        "[MaliQrStore] Historico salvo (%u item(ns))\n",
        static_cast<unsigned>(historyEntries.size())
    );
    return true;
}

} // namespace

namespace MaliQrStore {

bool begin(fs::FS *storage) {
    ensureMutex();
    StoreLock lock;
    if (!lock) return false;

    fs::FS *selectedStorage = storage ? storage : static_cast<fs::FS *>(&LittleFS);
    if (initialized && historyFs == selectedStorage) return historyFs != nullptr;

    if (initialized && dirty && !flushLocked()) return false;

    historyFs = selectedStorage;
    initialized = true;
    return loadHistoryLocked();
}

bool isReady() {
    ensureMutex();
    StoreLock lock;
    return lock && ensureInitializedLocked();
}

size_t favoriteCount() {
    ensureMutex();
    StoreLock lock;
    if (!lock) return 0;
    return bruceConfig.qrCodes.size();
}

bool favoriteAt(size_t index, FavoriteEntry &entryOut) {
    ensureMutex();
    StoreLock lock;
    if (!lock || index >= bruceConfig.qrCodes.size()) return false;

    const BruceConfig::QrCodeEntry &entry = bruceConfig.qrCodes[index];
    entryOut = {entry.menuName, entry.content};
    return true;
}

bool upsertFavorite(const String &name, const String &payload, String &errorOut) {
    errorOut = "";
    String normalizedName = name;
    normalizedName.trim();
    if (!validateLabel(normalizedName, false, &errorOut) ||
        !validateStoredPayload(payload, &errorOut)) {
        return false;
    }

    ensureMutex();
    StoreLock lock;
    if (!lock) {
        errorOut = F("Armazenamento indisponivel");
        return false;
    }

    size_t firstMatch = SIZE_MAX;
    bool changed = false;
    for (size_t index = 0; index < bruceConfig.qrCodes.size();) {
        BruceConfig::QrCodeEntry &entry = bruceConfig.qrCodes[index];
        if (entry.menuName != normalizedName) {
            ++index;
            continue;
        }

        if (firstMatch == SIZE_MAX) {
            firstMatch = index;
            if (entry.content != payload) {
                entry.content = payload;
                changed = true;
            }
            ++index;
        } else {
            bruceConfig.qrCodes.erase(bruceConfig.qrCodes.begin() + index);
            changed = true;
        }
    }

    if (firstMatch == SIZE_MAX) {
        if (bruceConfig.qrCodes.size() >= kMaxFavorites) {
            errorOut = F("Limite de 32 favoritos atingido");
            return false;
        }
        bruceConfig.qrCodes.push_back({normalizedName, payload});
        changed = true;
    }

    if (changed) bruceConfig.saveFile();
    return true;
}

bool removeFavorite(const String &name, String &errorOut) {
    errorOut = "";
    String normalizedName = name;
    normalizedName.trim();
    if (!validateLabel(normalizedName, false, &errorOut)) return false;

    ensureMutex();
    StoreLock lock;
    if (!lock) {
        errorOut = F("Armazenamento indisponivel");
        return false;
    }

    const auto match = std::find_if(
        bruceConfig.qrCodes.begin(),
        bruceConfig.qrCodes.end(),
        [&normalizedName](const BruceConfig::QrCodeEntry &entry) {
            return entry.menuName == normalizedName;
        }
    );
    if (match == bruceConfig.qrCodes.end()) {
        errorOut = F("Favorito nao encontrado");
        return false;
    }

    bruceConfig.removeQrCodeEntry(normalizedName);
    return true;
}

size_t historyCount() {
    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) return 0;
    return historyEntries.size();
}

bool historyAt(size_t index, HistoryEntry &entryOut) {
    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked() || index >= historyEntries.size()) return false;

    entryOut = historyEntries[index];
    return true;
}

bool lastHistory(HistoryEntry &entryOut) { return historyAt(0, entryOut); }

RecordResult recordSuccessfulDisplay(
    const String &payload, const String &label, String *errorOut
) {
    if (errorOut) *errorOut = "";
    if (!validateStoredPayload(payload, errorOut) || !validateLabel(label, true, errorOut)) {
        return RecordResult::Invalid;
    }

    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) {
        setError(errorOut, F("Armazenamento indisponivel"));
        return RecordResult::StorageUnavailable;
    }

    if (!historyEntries.empty() && historyEntries.front().payload == payload) {
        return RecordResult::Duplicate;
    }

    historyEntries.insert(historyEntries.begin(), {label, payload});
    trimHistoryLocked();
    markDirtyLocked();
    return RecordResult::Recorded;
}

size_t historyLimit() {
    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) return kDefaultHistoryLimit;
    return configuredHistoryLimit;
}

bool setHistoryLimit(size_t limit, String &errorOut) {
    errorOut = "";
    if (limit < 1 || limit > kMaxHistoryLimit) {
        errorOut = F("Limite deve estar entre 1 e 10");
        return false;
    }

    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) {
        errorOut = F("Armazenamento indisponivel");
        return false;
    }
    if (configuredHistoryLimit == limit) return true;

    configuredHistoryLimit = limit;
    trimHistoryLocked();
    markDirtyLocked();
    return true;
}

bool clearHistory() {
    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) return false;

    historyEntries.clear();
    markDirtyLocked();
    return true;
}

bool isDirty() {
    ensureMutex();
    StoreLock lock;
    return lock && dirty;
}

void service() {
    ensureMutex();
    StoreLock lock;
    if (!lock || !initialized || !dirty) return;
    if (static_cast<uint32_t>(millis() - dirtySince) >= kFlushIntervalMs) flushLocked();
}

bool flush() {
    ensureMutex();
    StoreLock lock;
    if (!lock || !ensureInitializedLocked()) return false;
    return flushLocked();
}

const char *historyPath() { return kHistoryPath; }

} // namespace MaliQrStore
