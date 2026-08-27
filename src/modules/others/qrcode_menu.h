#ifndef QR_CODE_MENU_H
#define QR_CODE_MENU_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

enum class QrDisplayResult : uint8_t {
    Displayed,
    EncoderBusy,
    EncodeFailed,
};

void qrcode_display(const String &qrcodeUrl);
QrDisplayResult qrcode_display_try(const String &qrcodeUrl, TickType_t timeoutTicks);
void pix_qrcode();
void qrcode_menu();
void custom_qrcode_menu();
void display_custom_qrcode();
void save_and_display_qrcode();
void remove_custom_qrcode();

#endif
