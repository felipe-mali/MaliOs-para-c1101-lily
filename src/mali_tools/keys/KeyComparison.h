#pragma once
#include "KeyProfile.h"
namespace MaliKeys {
constexpr int positionCount(const KeyProfile &p) {
    if (p.type == KeyType::Flat) return p.flat.positions;
    int total = 0;
    for (const auto &f : p.cross.faces) { if (!f.positions) return 0; total += f.positions; }
    return total;
}
}
