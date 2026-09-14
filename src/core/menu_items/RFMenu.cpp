#include "mali_tools/counter/counter_main.h"
#include "RFMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/mali/MaliWiki.h"
#include "modules/rf/record.h"
#include "modules/rf/rf_bruteforce.h"
#include "modules/rf/rf_jammer.h"
#include "modules/rf/rf_listen.h"
#include "modules/rf/rf_scan.h"
#include "modules/rf/rf_send.h"
#include "modules/rf/rf_utils.h"
#include "modules/rf/rf_spectrum.h"
#include "modules/rf/rf_waterfall.h"

void RFMenu::optionsMenu() {
    options = {
        {"RF Counter", CounterSuite::rfMenu},
        {"Procurar/copiar", [=]() { RFScan(); }       },
#if !defined(LITE_VERSION)
        {"Gravar RAW",      rf_raw_record             }, // Pablo-Ortiz-Lopez
        {"SubGHz personalizado", sendCustomRF         },
#endif
        {"Espectro",        rf_spectrum               },
#if !defined(LITE_VERSION)
        {"Espectro RSSI",   rf_CC1101_rssi            }, // @Pirata
        {"Espectro quadrado", rf_SquareWave           }, // @Pirata
        {"Espectrograma",   rf_waterfall              }, // dev_eclipse
#if defined(BUZZ_PIN) or defined(HAS_NS4168_SPKR) and defined(RF_LISTEN_H)
        {"Escutar",         rf_listen                 }, // dev_eclipse
#endif
        {"Forca bruta",     rf_bruteforce             }, // dev_eclipse
        {"Jammer",          [=]() { RFJammer(true); } },
#endif
        {"Configurar",      [this]() { configMenu(); }},
    };
    addOptionToMainMenu();
    options.push_back({"? Ajuda", []() { MaliWiki::open(MaliWiki::Category::SUB_GHZ); }});

    delay(200);
    String txt = "Radiofrequencia";
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) txt += " (CC1101)"; // Indicates if CC1101 is connected
    else txt += " Tx: " + String(bruceConfigPins.rfTx) + " Rx: " + String(bruceConfigPins.rfRx);

    loopOptions(options, MENU_TYPE_GEAR, txt.c_str());
}

void RFMenu::configMenu() {
    options = {
        {"Pino TX RF", lambdaHelper(gsetRfTxPin, true)},
        {"Pino RX RF", lambdaHelper(gsetRfRxPin, true)},
        {"Modulo RF", setRFModuleMenu},
        {"Frequencia RF", setRFFreqMenu},
        {"Voltar", [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_GEAR, "Config. RF");
}

void RFMenu::drawIcon(float scale) {
    clearIconArea();
    int radius = scale * 7;
    int deltaRadius = scale * 10;
    int triangleSize = scale * 30;

    if (triangleSize % 2 != 0) triangleSize++;

    // Body
    tft.fillCircle(iconCenterX, iconCenterY - radius, radius, bruceConfig.priColor);
    tft.fillTriangle(
        iconCenterX,
        iconCenterY,
        iconCenterX - triangleSize / 2,
        iconCenterY + triangleSize,
        iconCenterX + triangleSize / 2,
        iconCenterY + triangleSize,
        bruceConfig.priColor
    );

    // Left Arcs
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius,
        2 * radius,
        40,
        140,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius + deltaRadius,
        2 * radius + deltaRadius,
        40,
        140,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius + 2 * deltaRadius,
        2 * radius + 2 * deltaRadius,
        40,
        140,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );

    // Right Arcs
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius,
        2 * radius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius + deltaRadius,
        2 * radius + deltaRadius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY - radius,
        2.5 * radius + 2 * deltaRadius,
        2 * radius + 2 * deltaRadius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
}
