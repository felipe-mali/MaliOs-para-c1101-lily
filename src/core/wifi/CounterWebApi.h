#pragma once
#include <ESPAsyncWebServer.h>
void registerCounterWebApi(AsyncWebServer &server, bool (*authenticate)(AsyncWebServerRequest *));
