#pragma once

namespace MaliUI {
enum MenuCategory { Network = 0, Radio, Tools, Counter, Files, System };
constexpr bool sameName(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
// Configuration/CLI names remain stable. New registrations always have a route.
constexpr MenuCategory categoryFor(const char *name) {
    if (sameName(name, "WiFi") || sameName(name, "BLE") || sameName(name, "Ethernet") ||
        sameName(name, "Connect")) return Network;
    if (sameName(name, "RF") || sameName(name, "NRF24") || sameName(name, "LoRa") ||
        sameName(name, "FM") || sameName(name, "IR") || sameName(name, "RFID") ||
        sameName(name, "GPS")) return Radio;
    if (sameName(name, "Mali Counter")) return Counter;
    if (sameName(name, "Files")) return Files;
    if (sameName(name, "Config") || sameName(name, "Clock")) return System;
    return Tools;
}
} // namespace MaliUI
