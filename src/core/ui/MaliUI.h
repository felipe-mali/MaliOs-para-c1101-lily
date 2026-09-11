#pragma once
#include "MaliMotion.h"
#include "MaliTheme.h"
#include <globals.h>
namespace MaliUI {
enum class Icon { Mali, Network, Radio, Tools, Counter, Files, System, Wifi, Ble, Rf, Nfc, Ir, Settings };
Icon iconFor(const String &label);
const char *description(const String &label);
void drawIcon(Icon icon,int x,int y,int size,uint16_t color=ACCENT);
void drawHeader(const String &section="", bool status=true);
void drawFooter(const String &text="Turn: select   Click: open   Hold: back");
void drawCard(int x,int y,int w,int h,bool selected=false);
void drawMenuItem(const String &label,int x,int y,int w,int h,bool selected,bool enabled=true);
void drawProgress(int x,int y,int w,int value,int total,uint16_t color=ACCENT);
void drawDialog(const String &message,uint16_t statusColor=ACCENT);
void drawToast(const String &message,uint16_t statusColor=ACCENT);
void drawSelector(const char *const *labels,int count,int selected,int x,int y,int w);
void drawTabs(const char *const *labels,int count,int selected,int y);
void drawBoot(int progress,bool ready=false);
// A 24-row RGB565 strip; released before opening an app or entering a nested menu.
bool beginGear();
void endGear();
void drawGearMenu(const std::vector<Option> &items,int index,const GearMotion &motion,const char *section);
}
