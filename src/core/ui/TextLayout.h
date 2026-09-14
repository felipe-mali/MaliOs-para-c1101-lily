#pragma once
#include <stddef.h>
namespace MaliUI {
struct TextBreak { size_t length; size_t next; };
// Pixel fonts have a fixed cell width. Preserve bytes at a forced boundary.
constexpr TextBreak textBreak(const char *text, size_t length, size_t width) {
    if (!width || !length) return {0, length};
    const size_t limit = length < width ? length : width;
    for (size_t i = 0; i <= limit && i < length; ++i)
        if (text[i] == '\n') return {i, i + 1};
    if (length <= width) return {length, length};
    for (size_t i = limit; i > 0; --i) {
        if (text[i] == ' ') return {i, i + 1};
        if (text[i - 1] == '-' || text[i - 1] == '_') return {i, i};
    }
    return {limit, limit};
}
} // namespace MaliUI
