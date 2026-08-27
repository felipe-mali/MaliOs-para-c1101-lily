#ifndef MALI_QR_STORE_H
#define MALI_QR_STORE_H

#include <Arduino.h>
#include <FS.h>

namespace MaliQrStore {

constexpr size_t kDefaultHistoryLimit = 5;
constexpr size_t kMaxHistoryLimit = 10;
constexpr size_t kMaxFavorites = 32;
constexpr uint32_t kFlushIntervalMs = 60000;

struct FavoriteEntry {
    String name;
    String payload;
};

struct HistoryEntry {
    String label;
    String payload;
};

enum class RecordResult : uint8_t {
    Recorded,
    Duplicate,
    Invalid,
    StorageUnavailable,
};

// Loads the history from an already-mounted filesystem. Passing nullptr uses
// LittleFS, matching Bruce's canonical configuration storage. A mounted SD
// filesystem can be supplied explicitly when desired.
bool begin(fs::FS *storage = nullptr);
bool isReady();

// Favorites intentionally remain in bruceConfig.qrCodes. These accessors copy
// entries so callers never retain references across WebUI/UI task boundaries.
size_t favoriteCount();
bool favoriteAt(size_t index, FavoriteEntry &entryOut);
bool upsertFavorite(const String &name, const String &payload, String &errorOut);
bool removeFavorite(const String &name, String &errorOut);

// History is newest-first. Only call recordSuccessfulDisplay after the QR was
// actually shown; previews must not call it. The payload is opaque: callers
// remain responsible for obtaining consent before persisting Wi-Fi secrets.
size_t historyCount();
bool historyAt(size_t index, HistoryEntry &entryOut);
bool lastHistory(HistoryEntry &entryOut);
RecordResult recordSuccessfulDisplay(
    const String &payload, const String &label = "", String *errorOut = nullptr
);

size_t historyLimit();
bool setHistoryLimit(size_t limit, String &errorOut);
bool clearHistory();
bool isDirty();

// service() performs the deferred write after kFlushIntervalMs. It is cheap to
// call from the regular firmware loop. flush() forces a pending write and is
// intended for orderly WebUI/tool shutdown paths.
void service();
bool flush();

const char *historyPath();

} // namespace MaliQrStore

#endif
