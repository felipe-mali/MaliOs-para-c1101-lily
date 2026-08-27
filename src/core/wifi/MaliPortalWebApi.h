#pragma once

#include <ESPAsyncWebServer.h>

using MaliPortalWebAuthCallback = bool (*)(AsyncWebServerRequest *request);

// Authenticated Portal Studio API backed by LittleFS:/MaliOS/portals.
void registerMaliPortalWebApi(AsyncWebServer &webServer, MaliPortalWebAuthCallback authenticate);
