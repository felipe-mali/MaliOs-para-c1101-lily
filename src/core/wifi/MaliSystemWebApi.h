#pragma once

#include <ESPAsyncWebServer.h>

using MaliSystemWebAuthCallback = bool (*)(AsyncWebServerRequest *request);

// Registers the read-only, authenticated Mali Dashboard endpoint.
void registerMaliSystemWebApi(AsyncWebServer &webServer, MaliSystemWebAuthCallback authenticate);
