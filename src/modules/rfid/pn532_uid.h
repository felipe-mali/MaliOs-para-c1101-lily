#pragma once
#include <stddef.h>
#include <stdint.h>
// Validate the portion needed for UID-only polling before copying any external length.
// ATS may follow; this parser deliberately does not interpret it or read card blocks.
constexpr uint8_t pn532UidLength(const uint8_t *frame, size_t available) {
    if (!frame || available < 13 || frame[0] != 0 || frame[1] != 0 || frame[2] != 0xff ||
        uint8_t(frame[3] + frame[4]) != 0 || frame[5] != 0xd5 || frame[6] != 0x4b || frame[7] != 1)
        return 0;
    const uint8_t length = frame[12];
    return (length == 4 || length == 7 || length == 10) && frame[3] >= 8 + length &&
                   available >= size_t(13 + length)
               ? length
               : 0;
}
