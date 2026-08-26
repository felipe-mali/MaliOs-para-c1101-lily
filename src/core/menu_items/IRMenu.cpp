#include "IRMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/mali/MaliWiki.h"
#include "modules/ir/TV-B-Gone.h"
#include "modules/ir/custom_ir.h"
#include "modules/ir/ir_jammer.h"
#include "modules/ir/ir_read.h"

void IRMenu::optionsMenu() {
#if defined(ARDUINO_M5STICK_S3)
    bool prevPower = M5.Power.getExtOutput();
    M5.Power.setExtOutput(true); // ENABLE 5V OUTPUT
#endif
    options = {
        {"TV-B-Gone", StartTvBGone              },
        {"IR personalizado", otherIRcodes              },
        {"Ler IR",            [=]() { IrRead(); }       },
#if !defined(LITE_VERSION)
        {"Jammer IR", startIrJammer             }, // Simple frequency-adjustable jammer
#endif
        {"Configurar", [this]() { configMenu(); }},
    };
    addOptionToMainMenu();
    options.push_back({"? Ajuda", []() { MaliWiki::open(MaliWiki::Category::IR); }});

    String txt = "Infravermelho";
    txt += " Tx: " + String(bruceConfigPins.irTx) + " Rx: " + String(bruceConfigPins.irRx) +
           " Repet: " + String(bruceConfigPins.irTxRepeats);
    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
#if defined(ARDUINO_M5STICK_S3)
    M5.Power.setExtOutput(prevPower);
#endif
}

void IRMenu::configMenu() {
    options = {
        {"Pino TX IR", lambdaHelper(gsetIrTxPin, true)},
        {"Pino RX IR", lambdaHelper(gsetIrRxPin, true)},
        {"Repeticoes TX", setIrTxRepeats},
        {"Voltar", [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Config. IR");
}

void IRMenu::drawIcon(float scale) {
    clearIconArea();
    int iconSize = scale * 60;
    int radius = scale * 7;
    int deltaRadius = scale * 10;

    if (iconSize % 2 != 0) iconSize++;

    tft.fillRect(
        iconCenterX - iconSize / 2, iconCenterY - iconSize / 2, iconSize / 6, iconSize, bruceConfig.priColor
    );
    tft.fillRect(
        iconCenterX - iconSize / 3,
        iconCenterY - iconSize / 3,
        iconSize / 6,
        2 * iconSize / 3,
        bruceConfig.priColor
    );

    tft.drawCircle(iconCenterX - iconSize / 6, iconCenterY, radius, bruceConfig.priColor);

    tft.drawArc(
        iconCenterX - iconSize / 6,
        iconCenterY,
        2.5 * radius,
        2 * radius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX - iconSize / 6,
        iconCenterY,
        2.5 * radius + deltaRadius,
        2 * radius + deltaRadius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
    tft.drawArc(
        iconCenterX - iconSize / 6,
        iconCenterY,
        2.5 * radius + 2 * deltaRadius,
        2 * radius + 2 * deltaRadius,
        220,
        320,
        bruceConfig.priColor,
        bruceConfig.bgColor
    );
}
