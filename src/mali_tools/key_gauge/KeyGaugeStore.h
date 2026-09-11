#pragma once
#include "KeyGauge.h"
#include <Arduino.h>
#include <vector>
namespace KeyGauge {
bool validName(const String &name);
bool validProfile(const KeyGaugeProfile &profile);
// HTTP-compatible status codes; no UI side effects. Shared by device and WebUI.
int listProfiles(std::vector<String> &names, String &nextName);
int readProfile(const String &name, KeyGaugeProfile &profile);
int writeProfile(const KeyGaugeProfile &profile, bool replace = false);
int deleteProfile(const String &name);
}
