#include "../../src/core/ui/MenuRoute.h"
#include "../../src/core/ui/TextLayout.h"
#include "../../src/core/ui/PtBr.h"
using namespace MaliUI;
static_assert(categoryFor("Connect") == Network, "Connect stays reachable");
static_assert(categoryFor("Mali Counter") == Counter, "Restore the registered Counter menu");
static_assert(categoryFor("WiFi") == Network && categoryFor("BLE") == Network && categoryFor("Ethernet") == Network);
static_assert(categoryFor("RF") == Radio && categoryFor("IR") == Radio && categoryFor("RFID") == Radio);
static_assert(categoryFor("NRF24") == Radio && categoryFor("LoRa") == Radio && categoryFor("FM") == Radio && categoryFor("GPS") == Radio);
static_assert(categoryFor("Mali Tools") == Tools && categoryFor("Others") == Tools && categoryFor("JS Interpreter") == Tools);
static_assert(categoryFor("Files") == Files && categoryFor("Clock") == System && categoryFor("Config") == System);
static_assert(categoryFor("future registered app") == Tools, "A new registration has a fallback route");
static_assert(textBreak("abcdefgh", 8, 4).length == 4 && textBreak("abcdefgh", 8, 4).next == 4, "No lost byte on forced wrap");
static_assert(textBreak("abc def", 7, 4).length == 3 && textBreak("abc def", 7, 4).next == 4, "Word boundary");
static_assert(textBreak("abc\ndef", 7, 6).length == 3 && textBreak("abc\ndef", 7, 6).next == 4, "Explicit newline");
static_assert(textBreak("\nabc", 4, 4).length == 0 && textBreak("\nabc", 4, 4).next == 1, "Empty line");
static_assert(textBreak("abc-def", 7, 4).length == 4 && textBreak("abc-def", 7, 4).next == 4, "Keep the hyphen");
static_assert(textBreak("abc", 3, 0).next == 3, "Zero-width guard terminates");
