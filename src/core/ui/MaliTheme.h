#pragma once
#include <stdint.h>
namespace MaliUI {
constexpr uint16_t rgb(uint32_t hex) { return ((hex >> 8) & 0xf800) | ((hex >> 5) & 0x07e0) | ((hex >> 3) & 0x001f); }
constexpr uint16_t BACKGROUND=rgb(0x08090c), SURFACE=rgb(0x151218), SURFACE_ALT=rgb(0x21151e);
constexpr uint16_t BORDER=rgb(0x392936), TEXT_PRIMARY=rgb(0xf0edf3), TEXT_SECONDARY=rgb(0x9c939f);
constexpr uint16_t ACCENT=rgb(0xb18ad8), ACCENT_DIM=rgb(0x66457d), SUCCESS=rgb(0x82c69b);
constexpr uint16_t WARNING=rgb(0xd7ac68), ERROR=rgb(0xdf7888), TEXT_DISABLED=rgb(0x635b68);
constexpr int MALI_RADIUS_SMALL=3, MALI_RADIUS_MEDIUM=6, MALI_RADIUS_LARGE=10;
constexpr int TITLE=2, SECTION=1, PRIMARY=1, SECONDARY=1, VALUE=2, FOOTER=1;
}
