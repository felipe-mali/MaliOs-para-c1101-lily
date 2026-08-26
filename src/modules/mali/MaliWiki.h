#ifndef __MALI_WIKI_H__
#define __MALI_WIKI_H__

#include <stddef.h>
#include <stdint.h>

namespace MaliWiki {

enum class Category : uint8_t {
    WIFI,
    BLE,
    SUB_GHZ,
    NRF24,
    LORA,
    FM_RADIO,
    IR,
    ETHERNET,
    GPS,
    RFID,
    FILES,
    SCRIPTS,
    CLOCK,
    OTHERS,
    USB_HID,
    MALI_TOOLS,
    MALI_COUNTER,
};

void open(Category category);
const char *categoryName(Category category);
size_t documentedToolCount();

} // namespace MaliWiki

#endif
