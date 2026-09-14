#pragma once
#include "KeyProfile.h"
#include <Arduino.h>
namespace MaliKeys {
String millimetres(Measure value, bool unit = true);
String difference(Measure a, Measure b);
bool editNumber(const char *label, int &value, int lo, int hi, int step = 1, bool mm = false);
bool editText(const char *label, char *buffer, size_t size);
void showText(const char *title, const String &text);
}
