#pragma once
#include <stdint.h>

namespace MaliUI {
constexpr int wrap(int64_t value, int count) {
    return count > 0 ? int((value % count + count) % count) : 0;
}
// One retargetable transition, never a queue. Index is committed at input time.
struct GearMotion {
    int offset = 0, origin = 0, phase = 0, phaseOrigin = 0, phaseTarget = 0;
    uint32_t started = 0, duration = 120;
    bool active = false;
    constexpr void tick(uint32_t now) {
        if (!active) return;
        uint32_t elapsed = now - started;
        if (elapsed >= duration) { offset = 0; phase = phaseTarget; active = false; return; }
        int remain = 1024 - int(elapsed * 1024 / duration);
        int ease = int(int64_t(remain) * remain * remain / (1024 * 1024));
        offset = origin * ease / 1024;
        phase = phaseTarget + (phaseOrigin - phaseTarget) * ease / 1024;
    }
    constexpr void move(int steps, uint32_t now) {
        tick(now);
        int direction = steps > 0 ? 1 : -1;
        origin = offset + direction * 1024;
        if (origin > 1024) origin = 1024;
        if (origin < -1024) origin = -1024;
        phaseOrigin = wrap(phase, 3600);
        phaseTarget = phaseOrigin - direction * 300;
        phase = phaseOrigin; offset = origin;
        duration = active || steps > 1 || steps < -1 ? 90 : 120;
        started = now; active = true;
    }
};
enum class ButtonEvent { None, Select, Back };
struct HoldButton {
    bool armed = false, down = false, held = false;
    uint32_t started = 0;
    constexpr ButtonEvent update(bool pressed, uint32_t now) {
        if (!armed) { if (!pressed) armed = true; return ButtonEvent::None; }
        if (pressed && !down) { down = true; held = false; started = now; }
        if (pressed && down && !held && now - started >= 650) { held = true; return ButtonEvent::Back; }
        if (!pressed && down) { down = false; return held ? ButtonEvent::None : ButtonEvent::Select; }
        return ButtonEvent::None;
    }
};
}
