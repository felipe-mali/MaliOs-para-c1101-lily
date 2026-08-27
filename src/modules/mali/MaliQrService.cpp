#include "MaliQrService.h"

#include "MaliQrStore.h"
#include "modules/others/qrcode_menu.h"
#include <cmath>
#include <cstring>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace {

struct PendingQrDisplay {
    char payload[MaliQrService::kMaxPayloadBytes + 1];
    char label[49];
    bool recordHistory;
};

StaticQueue_t displayQueueControl;
uint8_t displayQueueStorage[sizeof(PendingQrDisplay)];
QueueHandle_t displayQueue = nullptr;

StaticSemaphore_t encoderMutexControl;
SemaphoreHandle_t encoderMutex = nullptr;

portMUX_TYPE resourceInitMux = portMUX_INITIALIZER_UNLOCKED;

bool isContinuationByte(uint8_t value) { return (value & 0xC0U) == 0x80U; }

bool isValidUtf8(const char *data, size_t length) {
    size_t index = 0;
    while (index < length) {
        const uint8_t first = static_cast<uint8_t>(data[index]);
        if (first == 0) return false; // The fixed queue uses NUL termination.

        if (first <= 0x7F) {
            index++;
            continue;
        }

        if (first >= 0xC2 && first <= 0xDF) {
            if (index + 1 >= length || !isContinuationByte(static_cast<uint8_t>(data[index + 1]))) {
                return false;
            }
            index += 2;
            continue;
        }

        if (first >= 0xE0 && first <= 0xEF) {
            if (index + 2 >= length) return false;
            const uint8_t second = static_cast<uint8_t>(data[index + 1]);
            const uint8_t third = static_cast<uint8_t>(data[index + 2]);
            if (!isContinuationByte(third)) return false;
            if (first == 0xE0) {
                if (second < 0xA0 || second > 0xBF) return false; // overlong sequence
            } else if (first == 0xED) {
                if (second < 0x80 || second > 0x9F) return false; // UTF-16 surrogate
            } else if (!isContinuationByte(second)) {
                return false;
            }
            index += 3;
            continue;
        }

        if (first >= 0xF0 && first <= 0xF4) {
            if (index + 3 >= length) return false;
            const uint8_t second = static_cast<uint8_t>(data[index + 1]);
            const uint8_t third = static_cast<uint8_t>(data[index + 2]);
            const uint8_t fourth = static_cast<uint8_t>(data[index + 3]);
            if (!isContinuationByte(third) || !isContinuationByte(fourth)) return false;
            if (first == 0xF0) {
                if (second < 0x90 || second > 0xBF) return false; // overlong sequence
            } else if (first == 0xF4) {
                if (second < 0x80 || second > 0x8F) return false; // above U+10FFFF
            } else if (!isContinuationByte(second)) {
                return false;
            }
            index += 4;
            continue;
        }

        return false;
    }
    return true;
}

uint16_t crcCcittUpdate(uint16_t crc, uint8_t data) {
    crc = static_cast<uint8_t>(crc >> 8) | (crc << 8);
    crc ^= data;
    crc ^= static_cast<uint8_t>(crc & 0xFFU) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0x00FFU) << 5;
    return crc;
}

String calculatePixCrc(const String &input) {
    uint16_t crc = 0xFFFF;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(input.c_str());
    for (size_t i = 0; i < input.length(); i++) crc = crcCcittUpdate(crc, data[i]);

    String result(crc, HEX);
    result.toUpperCase();
    while (result.length() < 4) result = "0" + result;
    return result;
}

bool isValidAmountSyntax(const String &amount) {
    bool hasDigit = false;
    bool hasDecimalPoint = false;
    for (size_t i = 0; i < amount.length(); i++) {
        const char value = amount.charAt(i);
        if (value >= '0' && value <= '9') {
            hasDigit = true;
        } else if (value == '.' && !hasDecimalPoint) {
            hasDecimalPoint = true;
        } else {
            return false;
        }
    }
    return hasDigit;
}

} // namespace

namespace MaliQrService {

void begin() {
    if (displayQueue && encoderMutex) return;

    portENTER_CRITICAL(&resourceInitMux);
    if (!displayQueue) {
        displayQueue = xQueueCreateStatic(
            1, sizeof(PendingQrDisplay), displayQueueStorage, &displayQueueControl
        );
    }
    if (!encoderMutex) encoderMutex = xSemaphoreCreateMutexStatic(&encoderMutexControl);
    portEXIT_CRITICAL(&resourceInitMux);

    if (!displayQueue || !encoderMutex) {
        Serial.println("[MaliQr] Falha ao inicializar recursos estaticos");
    }
}

bool validatePayload(const String &payload, String &errorOut) {
    errorOut = "";
    const size_t length = payload.length();
    if (length == 0) {
        errorOut = "O QR Code nao pode estar vazio";
        return false;
    }
    if (length > kMaxPayloadBytes) {
        errorOut = "Payload excede o limite de 154 bytes";
        return false;
    }
    if (!isValidUtf8(payload.c_str(), length)) {
        errorOut = "Payload nao contem UTF-8 valido";
        return false;
    }
    return true;
}

QueueResult enqueueDisplay(const String &payload, bool recordHistory, const String &historyLabel) {
    const size_t length = payload.length();
    String validationError;
    if (!validatePayload(payload, validationError)) return QueueResult::Invalid;
    if (historyLabel.length() > 48) return QueueResult::Invalid;

    begin();
    if (!displayQueue) return QueueResult::Full;

    PendingQrDisplay pending = {};
    memcpy(pending.payload, payload.c_str(), length);
    pending.payload[length] = '\0';
    memcpy(pending.label, historyLabel.c_str(), historyLabel.length());
    pending.label[historyLabel.length()] = '\0';
    pending.recordHistory = recordHistory;

    if (xQueueSend(displayQueue, &pending, 0) != pdPASS) return QueueResult::Full;

    Serial.printf("[MaliQr] Exibicao enfileirada (%u bytes)\n", static_cast<unsigned>(length));
    return QueueResult::Accepted;
}

bool processPendingDisplay() {
    MaliQrStore::service();
    begin();
    if (!displayQueue) return false;

    PendingQrDisplay pending = {};
    if (xQueuePeek(displayQueue, &pending, 0) != pdPASS) return false;

    const size_t length = strnlen(pending.payload, sizeof(pending.payload));
    if (length == 0 || length > kMaxPayloadBytes) {
        Serial.println("[MaliQr] Pedido descartado por tamanho invalido");
        xQueueReceive(displayQueue, &pending, 0);
        return true;
    }

    Serial.printf("[MaliQr] Exibindo pedido (%u bytes)\n", static_cast<unsigned>(length));
    const QrDisplayResult result = qrcode_display_try(String(pending.payload), 0);
    if (result == QrDisplayResult::EncoderBusy) {
        // Keep the accepted request queued. It will be retried from the next
        // UI loop instead of being silently lost while a preview is encoding.
        return false;
    }

    xQueueReceive(displayQueue, &pending, 0);
    if (result == QrDisplayResult::Displayed && pending.recordHistory) {
        String storeError;
        const MaliQrStore::RecordResult recordResult = MaliQrStore::recordSuccessfulDisplay(
            String(pending.payload), String(pending.label), &storeError
        );
        if (recordResult == MaliQrStore::RecordResult::Invalid ||
            recordResult == MaliQrStore::RecordResult::StorageUnavailable) {
            Serial.println("[MaliQr] Historico nao registrado: " + storeError);
        }
    }
    return true;
}

bool lockEncoder(TickType_t timeoutTicks) {
    begin();
    return encoderMutex && xSemaphoreTake(encoderMutex, timeoutTicks) == pdTRUE;
}

void unlockEncoder() {
    if (encoderMutex) xSemaphoreGive(encoderMutex);
}

bool buildPixPayload(
    const String &key, const String &amount, String &payloadOut, String &errorOut
) {
    payloadOut = "";
    errorOut = "";

    if (key.isEmpty()) {
        errorOut = "Chave PIX obrigatoria";
        return false;
    }
    if (key.length() > 25) {
        errorOut = "Chave PIX excede 25 bytes";
        return false;
    }
    if (!isValidUtf8(key.c_str(), key.length())) {
        errorOut = "Chave PIX contem UTF-8 invalido";
        return false;
    }
    if (amount.isEmpty()) {
        errorOut = "Valor PIX obrigatorio";
        return false;
    }
    if (amount.length() > 10) {
        errorOut = "Valor PIX excede 10 caracteres";
        return false;
    }
    if (!isValidAmountSyntax(amount)) {
        errorOut = "Valor PIX invalido";
        return false;
    }

    const float numericAmount = amount.toFloat();
    if (!std::isfinite(numericAmount)) {
        errorOut = "Valor PIX invalido";
        return false;
    }

    // Preserve Bruce's original normalization (Arduino String(float), two
    // decimal places) so the same key/value produces the same EMV payload.
    const String normalizedAmount(numericAmount);
    const String keyLength = key.length() >= 10 ? String(key.length()) : "0" + String(key.length());
    const String amountLength = normalizedAmount.length() >= 10
                                    ? String(normalizedAmount.length())
                                    : "0" + String(normalizedAmount.length());
    const String merchantAccount = "0014BR.GOV.BCB.PIX01" + keyLength + key;
    const String payloadWithoutCrc =
        "00020126" + String(merchantAccount.length()) + merchantAccount + "52040000530398654" +
        amountLength + normalizedAmount + "5802BR5909Bruce PIX6014Rio de Janeiro62070503***6304";

    payloadOut = payloadWithoutCrc + calculatePixCrc(payloadWithoutCrc);
    if (payloadOut.length() > kMaxPayloadBytes) {
        payloadOut = "";
        errorOut = "Payload PIX excede o limite do QR";
        return false;
    }
    return true;
}

} // namespace MaliQrService
