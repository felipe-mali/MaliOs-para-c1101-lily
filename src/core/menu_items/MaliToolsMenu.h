#ifndef __MALI_TOOLS_MENU_H__
#define __MALI_TOOLS_MENU_H__

#include <MenuItemInterface.h>

class MaliToolsMenu : public MenuItemInterface {
public:
    MaliToolsMenu() : MenuItemInterface("Mali Tools") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    bool hasTheme() override { return false; }
    const String &themePath() override {
        static const String emptyPath;
        return emptyPath;
    }

private:
    void quickAccessMenu();
};

#endif
