#ifndef __LED_CONTROL_H__
#define __LED_CONTROL_H__
#include <globals.h>

enum class MaliLedState : uint8_t {
    OFF,
    IDLE,
    MENU,
    LOADING,
    SUCCESS,
    ERROR,
    NFC_SCAN,
    RF_SCAN,
    COUNTER_SIGNAL,
    COUNTER_STRONG,
};

#ifdef HAS_RGB_LED
#include <Arduino.h>
#include <FastLED.h>

#define LED_EFFECT_SOLID 0
#define LED_COLOR_BREATHE 1
#define LED_EFFECT_COLOR_CYCLE 2
#define LED_EFFECT_COLOR_WHEEL 3
#define LED_EFFECT_CHASE 4
#define LED_EFFECT_CHASE_TAIL 5
#define LED_EFFECT_RAINBOW_CHASE 6
#define LED_EFFECT_RAINBOW_BREATHE 7
#define LED_EFFECT_DISCO 8
#define LED_EFFECT_FIRE 9

CRGB hsvToRgb(uint16_t h, uint8_t s, uint8_t v);
uint32_t alterOneColorChannel(uint32_t color, uint16_t newR, uint16_t newG, uint16_t newB);

void beginLed();
void blinkLed(int blinkTime = 50);

void setLedColor(CRGB color);
void setLedEffect(int effect);
void setLedColorConfig();
void setCustomColorMenu();
void setCustomColorSettingMenuR();
void setCustomColorSettingMenuG();
void setCustomColorSettingMenuB();
void setLedEffectConfig();
void setLedEffectSpeedConfig();
void setLedEffectDirectionConfig();
void ledSetup();
void ledEffects(bool enable);
void ledPreviewMode(bool enable);
void setLedBrightness(int value);
void setLedBrightnessConfig();

void setLedState(MaliLedState state);
MaliLedState getLedState();
void updateLedEffects();

class MaliLedStateGuard {
public:
    explicit MaliLedStateGuard(MaliLedState state);
    ~MaliLedStateGuard();

    MaliLedStateGuard(const MaliLedStateGuard &) = delete;
    MaliLedStateGuard &operator=(const MaliLedStateGuard &) = delete;

private:
    MaliLedState previousState;
};

#else
inline void blinkLed(int blinkTime = 50) {};
inline void setLedState(MaliLedState state) {}
inline MaliLedState getLedState() { return MaliLedState::OFF; }
inline void updateLedEffects() {}

class MaliLedStateGuard {
public:
    explicit MaliLedStateGuard(MaliLedState state) {}
};
#endif

#endif
