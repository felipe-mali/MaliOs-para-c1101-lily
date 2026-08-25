#ifndef __MALI_COUNTER_MENU_H__
#define __MALI_COUNTER_MENU_H__

#include <MenuItemInterface.h>

class MaliCounterMenu : public MenuItemInterface {
public:
    MaliCounterMenu() : MenuItemInterface("Mali Counter") {}
    void optionsMenu() override;
    void drawIcon(float scale) override;
    bool hasTheme() override { return false; }
    const String &themePath() override {
        static const String empty;
        return empty;
    }
};

#endif
