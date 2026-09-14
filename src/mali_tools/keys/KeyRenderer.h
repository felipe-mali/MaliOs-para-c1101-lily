#pragma once
#include "KeyProfile.h"
namespace MaliKeys {
class KeyRenderer {
public:
    static void begin(const KeyProfile &profile);
    // Called only on a change, never continuously while idle.
    static void draw(const KeyProfile &profile, uint8_t face, bool guides);
};
}
