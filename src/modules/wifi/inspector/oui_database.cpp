#include "oui_database.h"
namespace WifiInspector {
namespace {
struct Oui { uint32_t prefix; const char *name; };
// Small offline sample from Wireshark registry export, 2026-09-15.
// Source and exact registrants: tests/wifi_inspector/oui_source.json.
constexpr Oui entries[] = {
    {0x000048, "Epson"},
    {0x0000F0, "Samsung"},
    {0x000278, "Samsung"},
    {0x000393, "Apple"},
    {0x00047D, "Motorola"},
    {0x000502, "Apple"},
    {0x000512, "Zebra"},
    {0x00055D, "D-Link"},
    {0x00074D, "Zebra"},
    {0x0007AB, "Samsung"},
    {0x000A27, "Apple"},
    {0x000A28, "Motorola"},
    {0x000AEB, "TP-Link"},
    {0x000D88, "D-Link"},
    {0x000EC7, "Motorola"},
    {0x000F3D, "D-Link"},
    {0x0012F0, "Intel"},
    {0x001302, "Intel"},
    {0x001320, "Intel"},
    {0x001478, "TP-Link"},
    {0x00156D, "Ubiquiti"},
    {0x001570, "Zebra"},
    {0x001787, "Brother"},
    {0x001882, "Huawei"},
    {0x0019E0, "TP-Link"},
    {0x001A11, "Google"},
    {0x001A3F, "Intelbras"},
    {0x001BA9, "Brother"},
    {0x001E10, "Huawei"},
    {0x0022A1, "Huawei"},
    {0x0026AB, "Epson"},
    {0x002722, "Ubiquiti"},
    {0x004B12, "Espressif"},
    {0x0068EB, "HP"},
    {0x007007, "Espressif"},
    {0x007147, "Amazon"},
    {0x008077, "Brother"},
    {0x008621, "Amazon"},
    {0x009EC8, "Xiaomi"},
    {0x00BB3A, "Amazon"},
    {0x00C30A, "Xiaomi"},
    {0x00E04C, "Realtek"},
    {0x00EBD8, "Mercusys"},
    {0x00EC0A, "Xiaomi"},
    {0x00F620, "Google"},
    {0x00FB4A, "Espressif"},
    {0x04006E, "Google"},
    {0x040E3C, "HP"},
    {0x0418D6, "Ubiquiti"},
    {0x088AF1, "Mercusys"},
    {0x0C1C31, "Mercusys"},
    {0x10B676, "HP"},
    {0x180D2C, "Intelbras"},
    {0x24FD0D, "Intelbras"},
    {0x28CDC1, "Raspberry Pi"},
    {0x2CCF67, "Raspberry Pi"},
    {0x381A52, "Epson"},
    {0xFC934E, "Realtek"},
};
}
const char *manufacturer(const Mac &mac) {
    if(!validMac(mac)||privateMac(mac))return "Desconhecido";
    uint32_t prefix=uint32_t(mac[0])<<16|uint32_t(mac[1])<<8|mac[2];
    for(const auto &entry:entries)if(entry.prefix==prefix)return entry.name;
    return "Desconhecido";
}
}
