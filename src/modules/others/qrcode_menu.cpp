#include "qrcode_menu.h"
#include "../lib/TFT_eSPI_QRcode/src/qrcode.h"
#include "core/config.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/mali/MaliQrService.h"
#include "modules/mali/MaliQrStore.h"

QrDisplayResult qrcode_display_try(const String &qrcodeUrl, TickType_t timeoutTicks) {
#ifdef HAS_SCREEN
    if (!MaliQrService::lockEncoder(timeoutTicks)) return QrDisplayResult::EncoderBusy;

    QRcode qrcode(&tft);
    qrcode.init();
    const bool created = qrcode.create(qrcodeUrl);
    MaliQrService::unlockEncoder();

    if (!created) {
        Serial.println("[MaliQr] Memoria insuficiente para gerar QR");
        tft.fillScreen(bruceConfig.bgColor);
        displayError("Sem memoria para QR");
        delay(1200);
        return QrDisplayResult::EncodeFailed;
    }

    delay(300); // Due to M5 sel press, it could be confusing with next line
    while (!check(EscPress) && !check(SelPress)) delay(100);
    tft.fillScreen(bruceConfig.bgColor);
#endif
    return QrDisplayResult::Displayed;
}

void qrcode_display(const String &qrcodeUrl) {
    const QrDisplayResult result = qrcode_display_try(qrcodeUrl, pdMS_TO_TICKS(1000));
    if (result == QrDisplayResult::EncoderBusy) {
        Serial.println("[MaliQr] Encoder ocupado; exibicao cancelada");
        displayError("QR ocupado");
        delay(1000);
    } else if (result == QrDisplayResult::Displayed && !qrcodeUrl.startsWith("WIFI:")) {
        String storeError;
        const MaliQrStore::RecordResult recordResult =
            MaliQrStore::recordSuccessfulDisplay(qrcodeUrl, "", &storeError);
        if (recordResult == MaliQrStore::RecordResult::Invalid ||
            recordResult == MaliQrStore::RecordResult::StorageUnavailable) {
            Serial.println("[MaliQr] Historico nao registrado: " + storeError);
        }
    }
}

void display_custom_qrcode() {
    String message = keyboard("", 100, "QRCode text:");
    return qrcode_display(message);
}

void pix_qrcode() {
    String key = keyboard("", 25, "PIX Key:");
    if (key == "\x1B") return;
    String amount = num_keyboard("1000.00", 10, "Int amount:");
    if (amount == "\x1B") return;

    String payload;
    String error;
    if (!MaliQrService::buildPixPayload(key, amount, payload, error)) {
        Serial.println("[MaliQr] PIX invalido: " + error);
        displayError(error);
        delay(1200);
        return;
    }

    return qrcode_display(payload);
}

void qrcode_menu() {

    std::vector<Option> options;

    // Add QR codes from the config
    for (const auto &entry : bruceConfig.qrCodes) {
        options.push_back({entry.menuName.c_str(), lambdaHelper(qrcode_display, entry.content)});
    }

    options.push_back({"PIX", pix_qrcode});
    options.push_back({"Personalizado", custom_qrcode_menu});
    addOptionToMainMenu();

    loopOptions(options);
    options.clear();
}

void custom_qrcode_menu() {
    options = {
        {"Display",      display_custom_qrcode  },
        {"Save&Display", save_and_display_qrcode},
        {"Remove",       remove_custom_qrcode   },
        {"Voltar",       qrcode_menu            }
    };
    loopOptions(options);
}

void save_and_display_qrcode() {

    String name = keyboard("", 100, "QRCode name:");
    if (name == "\x1B") return;
    if (name.isEmpty()) {
        displayError("Nome nao pode ficar vazio!");
        delay(1000);
        return;
    }

    if (std::any_of(
            bruceConfig.qrCodes.begin(),
            bruceConfig.qrCodes.end(),
            [&](const BruceConfig::QrCodeEntry &entry) { return entry.menuName == name; }
        )) {
        displayError("Nome ja existe!");
        delay(1000);
        return;
    }

    String text = keyboard("", 100, "QRCode text:");
    if (text == "\x1B") return;

    bruceConfig.addQrCodeEntry(name, text);
    return qrcode_display(text);
}

void remove_custom_qrcode() {
    if (bruceConfig.qrCodes.empty()) {
        displayInfo("Nao ha nada para remover!");
        delay(1000);
        custom_qrcode_menu();
    }
    std::vector<Option> options;

    // Populate options with the QR codes from the config
    for (const auto &entry : bruceConfig.qrCodes) {
        options.emplace_back(entry.menuName.c_str(), [=]() {
            bruceConfig.removeQrCodeEntry(entry.menuName);
            log_i("Removed QR code: %s", entry.menuName.c_str());
            custom_qrcode_menu();
        });
    }

    options.emplace_back("Voltar", [=]() { custom_qrcode_menu(); });

    loopOptions(options);
}
