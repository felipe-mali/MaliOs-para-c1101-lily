#ifndef MALI_WIFI_WEB_API_H
#define MALI_WIFI_WEB_API_H

#include <ESPAsyncWebServer.h>
#include <functional>

using MaliWifiWebAuthCallback = std::function<bool(AsyncWebServerRequest *)>;

namespace MaliWifiWebApi {

void begin();
void registerRoutes(AsyncWebServer &server, MaliWifiWebAuthCallback authenticate);

// Executes at most one queued Wi-Fi administration action from a regular
// firmware/UI context. AsyncTCP callbacks only validate and enqueue requests.
void service();

} // namespace MaliWifiWebApi

#endif
