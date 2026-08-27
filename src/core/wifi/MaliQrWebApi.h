#pragma once

#include <ESPAsyncWebServer.h>

using MaliQrWebAuthCallback = bool (*)(AsyncWebServerRequest *request);

// Registers POST /api/qr/show and POST /api/qr/preview on the existing WebUI
// server. Both accept a form field named "payload"; PIX may instead send
// type=pix, key and amount. Preview returns one matrix-size byte followed by
// row-major, MSB-first packed matrix bits.
void registerMaliQrWebApi(AsyncWebServer &webServer, MaliQrWebAuthCallback authenticate);
