#ifndef MALI_QR_SERVICE_H
#define MALI_QR_SERVICE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

namespace MaliQrService {

// The bundled version-7/L encoder silently truncates byte-mode data above
// 154 bytes. Keep this as a byte limit: UTF-8 characters may use more than
// one byte.
constexpr size_t kMaxPayloadBytes = 154;

enum class QueueResult : uint8_t {
    Accepted,
    Full,
    Invalid,
};

// Creates the static queue and encoder mutex. It is safe to call repeatedly.
void begin();

// Checks non-empty content, the encoder's byte limit and well-formed UTF-8.
bool validatePayload(const String &payload, String &errorOut);

// Validates and copies a payload into the single-entry display queue.
// This function is safe to call from an AsyncWebServer task and never touches
// the TFT. A full queue is left unchanged.
QueueResult enqueueDisplay(const String &payload);

// Consumes at most one queued request and displays it from the caller's UI
// context. Returns true when a request was consumed.
bool processPendingDisplay();

// The bundled QR encoder uses shared global work buffers. All code that calls
// QRcode::create() or qrencode() must hold this mutex.
bool lockEncoder(TickType_t timeoutTicks);
void unlockEncoder();

// Builds the same fixed-name/fixed-city PIX payload used by Bruce's original
// PIX screen. Inputs are validated, while UI and persistence remain outside
// this pure builder.
bool buildPixPayload(
    const String &key, const String &amount, String &payloadOut, String &errorOut
);

} // namespace MaliQrService

#endif
