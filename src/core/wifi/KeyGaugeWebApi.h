#pragma once
#include <ESPAsyncWebServer.h>
void registerKeyGaugeWebApi(AsyncWebServer &server, bool (*authenticate)(AsyncWebServerRequest *));
