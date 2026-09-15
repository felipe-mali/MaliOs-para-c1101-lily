#pragma once
#include "device_model.h"
#include <Arduino.h>
#include <functional>
namespace WifiInspector {
struct ScanSettings { bool tcp=true,names=true,ssdp=true; uint16_t timeoutMs=120;uint32_t start=0; };
struct Progress { const char *phase;uint8_t percent;uint32_t ip;size_t found; };
using ProgressCallback=std::function<bool(const Progress &)>; // false cancels, called from UI task only
bool currentNetwork(Network &network);
String ipText(uint32_t ip);
String macText(const Mac &mac);
class DeviceScanner {
public:
    bool run(const Network &network,const ScanSettings &settings,ScanResult &result,const ProgressCallback &progress);
};
}
