#pragma once
#include "core/ui/MaliUI.h"
namespace MaliUI {
struct InputEvent { int32_t steps = 0; bool select = false, back = false; };
class Input {
    MaliUI::HoldButton button;
public:
    Input() {
        check(SelPress); check(NextPress); check(PrevPress);
#ifdef HAS_ENCODER
        drainRotarySteps();
#endif
    }
    InputEvent read() {
        InputEvent e;
        e.back = returnToMenu || check(EscPress);
#if defined(HAS_ENCODER) && defined(SEL_BTN)
        bool down = digitalRead(SEL_BTN) == BTN_ACT;
        bool wasDown = button.down, armed = button.armed;
        auto event = button.update(down, millis());
        bool virtualSelect = check(SelPress);
        e.back |= event == MaliUI::ButtonEvent::Back;
        e.select = event == MaliUI::ButtonEvent::Select || (!down && !wasDown && armed && virtualSelect);
#else
        e.select = check(SelPress);
#endif
#ifdef HAS_ENCODER
        int32_t rotary = drainRotarySteps();
        if (rotary) {
            // Same orientation as loopOptions: Next increases, Prev decreases.
            e.steps = rotary > 10000 ? -10000 : rotary < -10000 ? 10000 : -rotary;
            check(NextPress); check(PrevPress); check(UpPress); check(DownPress);
        } else
#endif
        {
            if (check(NextPress) || check(DownPress)) ++e.steps;
            if (check(PrevPress) || check(UpPress)) --e.steps;
        }
        if (e.back) e.select = false;
        return e;
    }
};
}
