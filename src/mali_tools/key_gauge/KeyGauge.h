#pragma once
#include <stdint.h>
namespace KeyGauge {
struct GaugePoint { uint8_t level = 0; };
struct KeyGaugeProfile {
    char name[32] = "";
    uint8_t points = 6;
    uint8_t thickness = 5;
    uint8_t width = 90;
    GaugePoint gaugePoints[10];
};
void open();
void queueWebPreview(const KeyGaugeProfile &profile);
bool processWebPreview();
}
