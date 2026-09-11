#include "core/ui/MaliUI.h"
#include "webInterface.h"
#include "MaliQrWebApi.h"
#include "KeyGaugeWebApi.h"
#include "CounterWebApi.h"
#include "mali_tools/counter/CounterLab.h"
#include "mali_tools/key_gauge/KeyGauge.h"
#include "MaliPortalWebApi.h"
#include "MaliSystemWebApi.h"
#include "MaliWifiWebApi.h"
#include "core/display.h"    // using displayRedStripe as error msg
#include "core/mykeyboard.h" // using keyboard when calling rename
#include "core/passwords.h"
#include "core/ram_profile.h"
#include "core/sd_functions.h" // using sd functions called to rename and manage sd files
#include "core/serialcmds.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h" // using common wifisetup
#include "esp_task_wdt.h"
#include "modules/mali/MaliQrService.h"
#include "modules/mali/MaliQrStore.h"
#include "webFiles.h"
#include <MD5Builder.h>
#include <cstddef>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <globals.h>

File uploadFile;
// WiFi as a Client
const int default_webserverporthttp = 80;

// WiFi as an Access Point
IPAddress AP_GATEWAY(172, 0, 0, 1); // Gateway

AsyncWebServer *server = nullptr; // initialise webserver
const char *host = "bruce";
static bool mdnsRunning = false;

namespace {
constexpr size_t WEB_PATH_MAX_BYTES = 255;
constexpr size_t WEB_FILENAME_MAX_BYTES = 96;
constexpr size_t WEB_EDITOR_MAX_BYTES = 64 * 1024;

struct WebUploadState {
    FS *fs = nullptr;
    bool failed = false;
    uint16_t status = 200;
    bool encryptedChunkWritten = false;
    char path[WEB_PATH_MAX_BYTES + 1] = {0};
    char message[80] = {0};
};

bool isSafeWebPath(const String &path, bool absolute) {
    if (path.isEmpty() || path.length() > WEB_PATH_MAX_BYTES) return false;
    if ((path[0] == '/') != absolute) return false;
    String segment;
    const size_t start = absolute ? 1 : 0;
    for (size_t i = start; i <= path.length(); ++i) {
        const char c = i < path.length() ? path[i] : '/';
        if (c == '/') {
            if (segment == "." || segment == "..") return false;
            if (segment.isEmpty() && i < path.length()) return false;
            segment = "";
            continue;
        }
        if (c == '\\' || static_cast<uint8_t>(c) < 0x20 || c == 0x7f) return false;
        segment += c;
    }
    return true;
}

bool isSafeWebFilename(const String &name) {
    return name.length() <= WEB_FILENAME_MAX_BYTES && isSafeWebPath(name, false) &&
           name.indexOf('/') < 0;
}

FS *selectWebFileSystem(const String &name) {
    if (name == "LittleFS") return &LittleFS;
    if (name == "SD") return setupSdCard() ? static_cast<FS *>(&SD) : nullptr;
    return nullptr;
}

String joinWebPath(const String &folder, const String &relativePath) {
    return folder == "/" ? "/" + relativePath : folder + "/" + relativePath;
}

void failUpload(WebUploadState *state, uint16_t status, const char *message) {
    if (!state || state->failed) return;
    state->failed = true;
    state->status = status;
    strlcpy(state->message, message, sizeof(state->message));
    if (state->fs && state->path[0] != '\0' && state->fs->exists(state->path))
        state->fs->remove(state->path);
}
} // namespace

// Generate random token
String generateToken(int length = 24) {
    String token = "";
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < length; i++) { token += charset[random(0, sizeof(charset) - 1)]; }
    return token;
}

/**********************************************************************
**  Function: stopWebUi
**  Turn off the WebUI
**********************************************************************/
void stopWebUi() {
    if (MaliQrStore::isDirty() && !MaliQrStore::flush()) {
        Serial.println("[MaliQrStore] Historico pendente nao foi salvo ao fechar WebUI");
    }
    tft.setLogging(false);
    isWebUIActive = false;
    server->end();
    server->~AsyncWebServer();
    free(server);
    server = nullptr;
    if (mdnsRunning) {
        MDNS.end();
        mdnsRunning = false;
    }
}

/**********************************************************************
**  Function: cleanlyStopWebUiForWiFiFeature
**  Cleanly stop WebUI and AP mode before starting a WiFi feature
**  This prevents WiFi mode conflicts when features need exclusive control
**********************************************************************/
void cleanlyStopWebUiForWiFiFeature() {
    // Only proceed if WebUI is active
    if (!isWebUIActive && !server) { return; }

    // Brief notification (non-blocking)
    Serial.println("Stopping WebUI for WiFi feature...");

    // Stop the WebUI
    if (server) {
        stopWebUi();
        // Give the web server time to fully shut down
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Disconnect WiFi AP mode if it's the WebUI's AP
    // Check if we're in AP or APSTA mode (used by WebUI)
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
        wifiDisconnect();
        // Give WiFi time to fully disconnect
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    Serial.println("WebUI stopped, starting WiFi feature...");
}
/**********************************************************************
**  Function: loopOptionsWebUi
**  Display options to launch the WebUI
**********************************************************************/
void loopOptionsWebUi() {
    if (isWebUIActive) {
        bool opt = WiFi.getMode() - 1;
        options = {
            {"Parar WebUI", stopWebUi},
            {"Tela WebUI", lambdaHelper(startWebUi, opt)}
        };
        addOptionToMainMenu();
        loopOptions(options);
        return;
    }
    options = {
        {"Minha rede", lambdaHelper(startWebUi, false)},
        {"Modo AP",    lambdaHelper(startWebUi, true) },
    };

    loopOptions(options);
    // On fail installing will run the following line
}

/**********************************************************************
**  Function: humanReadableSize
** Make size of files human readable
** source: https://github.com/CelliesProjects/minimalUploadAuthESP32
**********************************************************************/
String humanReadableSize(uint64_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " kB";
    else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
    else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

/**********************************************************************
**  Function: listFiles
**  list all of the files, if ishtml=true, return html rather than simple text
**********************************************************************/
String listFiles(FS &fs, const String &folder) {
    // log_i("Listfiles Start");
    String returnText = "pa:" + folder + ":0\n";
    // Serial.println("Listing files stored on SD");

    File root = fs.open(folder);
    if (!root || !root.isDirectory()) return "";

    while (true) {
        bool isDir;
        String fullPath = root.getNextFileName(&isDir);
        String nameOnly = fullPath.substring(fullPath.lastIndexOf("/") + 1);
        if (fullPath == "") { break; }
        // Serial.printf("Path: %s (isDir: %d)\n", fullPath.c_str(), isDir);

        if (esp_get_free_heap_size() > (String("Fo:" + nameOnly + ":0\n").length()) + 1024) {
            if (isDir) {
                // Serial.printf("Directory: %s\n", fullPath.c_str());
                returnText += "Fo:" + nameOnly + ":0\n";
            } else {
                // For files, we need to get the size, so we open the file briefly
                // Serial.printf("Opening file for size check: %s\n", fullPath.c_str());
                File file = fs.open(fullPath);
                // Serial.printf("File size: %llu bytes\n", file.size());
                if (file) {
                    returnText += "Fi:" + nameOnly + ":" + humanReadableSize(file.size()) + "\n";
                    file.close();
                }
            }
        } else break;
        delay(1);
    }
    root.close();
    // log_i("ListFiles End");
    return returnText;
}

/**********************************************************************
**  Function: checkUserWebAuth
** used by server->on functions to discern whether a user has the correct
** httpapitoken OR is authenticated by username and password
**********************************************************************/
bool checkUserWebAuth(AsyncWebServerRequest *request, bool onFailureReturnLoginPage = false) {
    if (request->hasHeader("Cookie")) {
        const AsyncWebHeader *cookie = request->getHeader("Cookie");
        String c = cookie->value();
        int idx = c.indexOf("BRUCESESSION=");
        if (idx != -1) {
            int start = idx + 13;
            int end = c.indexOf(';', start);
            if (end == -1) end = c.length();
            String token = c.substring(start, end);
            if (bruceConfig.isValidWebUISession(token)) { return true; }
        }
    }
    if (onFailureReturnLoginPage) {
        serveWebUIFile(request, "login.html", "text/html", true, login_html, login_html_size);
    } else {
        request->send(401, "text/plain", "Unauthorized");
    }
    return false;
}

/**********************************************************************
**  Function: createDirRecursive
** Create folders recursivelly
**********************************************************************/
void createDirRecursive(const String &path, FS fs) {
    String currentPath = "";
    int startIndex = 0;
    // Serial.print("Verifying folder: ");
    // Serial.println(path);

    while (startIndex < path.length()) {
        int endIndex = path.indexOf("/", startIndex);
        if (endIndex == -1) endIndex = path.length();

        currentPath += path.substring(startIndex, endIndex);
        if (currentPath.length() > 0) {
            if (!fs.exists(currentPath)) {
                fs.mkdir(currentPath);
                // Serial.print("Creating folder: ");
                // Serial.println(currentPath);
            }
        }

        if (endIndex < path.length()) { currentPath += "/"; }
        startIndex = endIndex + 1;
    }
}
/**********************************************************************
**  Function: handleUpload
** handles uploads to the filserver
**********************************************************************/
void handleUpload(
    AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final
) {
    if (!checkUserWebAuth(request)) return;
    WebUploadState *state = static_cast<WebUploadState *>(request->_tempObject);
    if (!index) {
        state = static_cast<WebUploadState *>(calloc(1, sizeof(WebUploadState)));
        request->_tempObject = state;
        if (!state) return;
        const String fsName = request->hasArg("fs") ? request->arg("fs") : "";
        const String folder = request->hasArg("folder") ? request->arg("folder") : "";
        state->fs = selectWebFileSystem(fsName);
        if (!state->fs) failUpload(state, 400, "Invalid or unavailable file system");
        if (!isSafeWebPath(folder, true)) failUpload(state, 400, "Invalid destination path");
        if (request->hasArg("password")) {
            if (request->arg("password").length() > 64)
                failUpload(state, 400, "Invalid encryption password");
            filename += ".enc";
        }
        if (!isSafeWebPath(filename, false)) failUpload(state, 400, "Invalid upload filename");
        if (state->failed) return;
        const String fullPath = joinWebPath(folder, filename);
        if (!isSafeWebPath(fullPath, true)) {
            failUpload(state, 400, "Invalid upload path");
            return;
        }
        strlcpy(state->path, fullPath.c_str(), sizeof(state->path));
        const String dirPath = fullPath.substring(0, fullPath.lastIndexOf('/'));
        if (!dirPath.isEmpty()) createDirRecursive(dirPath, *state->fs);
        request->_tempFile = state->fs->open(fullPath, "w");
        if (!request->_tempFile) failUpload(state, 500, "Failed to open upload destination");
    }
    if (!state || state->failed) return;
    if (len) {
        if (request->hasArg("password")) {
            if (state->encryptedChunkWritten || index != 0) {
                if (request->_tempFile) request->_tempFile.close();
                failUpload(state, 413, "Encrypted upload exceeds one chunk");
                return;
            }
            state->encryptedChunkWritten = true;
            String plaintext;
            plaintext.reserve(len);
            plaintext.concat(reinterpret_cast<const char *>(data), len);
            const String cyphertxt = encryptString(plaintext, request->arg("password"));
            if (cyphertxt.isEmpty() || !request->_tempFile ||
                request->_tempFile.write(
                    reinterpret_cast<const uint8_t *>(cyphertxt.c_str()), cyphertxt.length()
                ) != cyphertxt.length()) {
                if (request->_tempFile) request->_tempFile.close();
                failUpload(state, 500, "Failed to encrypt or write upload");
                return;
            }
        } else if (!request->_tempFile || request->_tempFile.write(data, len) != len) {
            if (request->_tempFile) request->_tempFile.close();
            failUpload(state, 500, "Failed to write upload");
            return;
        }
    }
    if (final && request->_tempFile) request->_tempFile.close();
}

void notFound(AsyncWebServerRequest *request) { request->send(404, "text/plain", "Nothing in here Sharky"); }

/**********************************************************************
**  Function: drawWebUiScreen
**  Draw information on screen of WebUI.
**********************************************************************/
void drawWebUiScreen(bool mode_ap) {
    tft.fillScreen(MaliUI::BACKGROUND);
    MaliUI::drawHeader("WEBUI");
    MaliUI::drawCard(8,32,tftWidth-16,tftHeight-56);

    String txt;
    if (!mode_ap) txt = WiFi.localIP().toString();
    else txt = WiFi.softAPIP().toString();

    int padX = 14;
    int currentY = 40;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);

    if (mode_ap) {
        tft.setCursor(padX, currentY);
        tft.print("Net: " + WiFi.softAPSSID());
        currentY += LH * FP + 6;
    }

    tft.setCursor(padX, currentY);
    tft.print("http://" + txt);
    currentY += LH * FP + 6;

    tft.setCursor(padX, currentY);
    tft.print("IP:  " + txt);
    currentY += LH * FP + 6;

    tft.setCursor(padX, currentY);
    tft.print("Usr: " + String(bruceConfig.webUI.user));
    currentY += LH * FP + 6;

    tft.setCursor(padX, currentY);
    tft.print("Pwd: " + String(bruceConfig.webUI.pwd));

    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.setTextSize(FP);
    MaliUI::drawFooter("BACK: WebUI options");

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

/**********************************************************************
**  Function: color565ToWebHex
**  convert 565 color to web hex format for theme purposes
**********************************************************************/
String color565ToWebHex(uint16_t color565) {
    // Extract RGB components from 565
    uint8_t r = (color565 >> 11) & 0x1F;
    uint8_t g = (color565 >> 5) & 0x3F;
    uint8_t b = color565 & 0x1F;

    // Scale up to 8 bits
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    return String(hex);
}

/**********************************************************************
**  Function: serveWebUIFile
**  serves files for WebUI and checks for custom WebUI files
**********************************************************************/
void serveWebUIFile(AsyncWebServerRequest *request, const String &filename, const char *contentType) {
    serveWebUIFile(request, filename, contentType, false, nullptr, 0);
}
void serveWebUIFile(
    AsyncWebServerRequest *request, const String &filename, const char *contentType, bool gzip,
    const uint8_t *originaFile, uint32_t originalFileSize
) {
    AsyncWebServerResponse *response = nullptr;
    FS *fs = NULL;
    if (setupSdCard()) {
        if (SD.exists("/BruceWebUI/" + filename)) fs = &SD;
    }
    // Keep SD as the preferred custom WebUI source, but still inspect
    // LittleFS when a mounted SD does not contain this particular file.
    if (!fs && LittleFS.exists("/BruceWebUI/" + filename)) fs = &LittleFS;
    if (fs) {
        response = request->beginResponse(*fs, "/BruceWebUI/" + filename, contentType);
    } else {
        if (filename == "theme.css") {
            String css = ":root{--color:" + color565ToWebHex(bruceConfig.priColor) +
                         ";--sec-color:" + color565ToWebHex(bruceConfig.secColor) +
                         ";--background:" + color565ToWebHex(bruceConfig.bgColor) + ";}";
            AsyncWebServerResponse *themeResponse = request->beginResponse(200, "text/css", css);
            request->send(themeResponse);
            return;
        }
        response = request->beginResponse(200, String(contentType), originaFile, originalFileSize);
        if (gzip) {
            if (!response->addHeader("Content-Encoding", "gzip")) log_e("Failed to add gzip header");
        }
    }
    request->send(response);
}

/**********************************************************************
**  Function: startMdnsResponder
**  Try to start mDNS only if there is enough internal heap available
**********************************************************************/
static bool startMdnsResponder() {
    RAM_LOG("before MDNS");

    if (!MDNS.begin(host)) {
        RAM_LOG("MDNS failed");
        Serial.printf("Error setting up MDNS responder!\n");
        return false;
    }

    RAM_LOG("after MDNS");
    return true;
}

/**********************************************************************
**  Function: configureWebServer
**  configure web server
**********************************************************************/
void configureWebServer() {
    mdnsRunning = startMdnsResponder();
    server->onNotFound(notFound);
    registerMaliQrWebApi(*server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); });
    registerMaliPortalWebApi(
        *server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); }
    );
    registerMaliSystemWebApi(
        *server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); }
    );
    MaliWifiWebApi::registerRoutes(
        *server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); }
    );

    registerKeyGaugeWebApi(*server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); });

    registerCounterWebApi(*server, [](AsyncWebServerRequest *request) { return checkUserWebAuth(request); });

    // Index
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request, true)) {
            serveWebUIFile(request, "index.html", "text/html", true, index_html, index_html_size);
        }
    });

    // Login
    server->on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->contentLength() > 256) {
            request->send(413, "text/plain; charset=utf-8", "Request too large");
            return;
        }
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();

            if (username.length() <= 32 && password.length() <= 64 &&
                username == bruceConfig.webUI.user && password == bruceConfig.webUI.pwd) {
                String token = generateToken();
                AsyncWebServerResponse *response = request->beginResponse(302);
                response->addHeader("Location", "/");
                response->addHeader(
                    "Set-Cookie", "BRUCESESSION=" + token + "; Path=/; HttpOnly; SameSite=Strict"
                );
                request->send(response);
                bruceConfig.addWebUISession(token);
                return;
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/?failed");
        request->send(response);
    });

    // Logout
    server->on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasHeader("Cookie")) {
            const AsyncWebHeader *cookie = request->getHeader("Cookie");
            String c = cookie->value();
            int idx = c.indexOf("BRUCESESSION=");
            if (idx != -1) {
                int start = idx + 13;
                int end = c.indexOf(';', start);
                if (end == -1) end = c.length();
                String token = c.substring(start, end);
                bruceConfig.removeWebUISession(token);
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/?loggedout");
        response->addHeader(
            "Set-Cookie",
            "BRUCESESSION=0; Path=/; HttpOnly; SameSite=Strict; Expires=Thu, 01 Jan 1970 00:00:00 GMT"
        );
        request->send(response);
    });

    // Static files
    server->on("/theme.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "theme.css", "text/css");
    });
    server->on("/index.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "index.css", "text/css", true, index_css, index_css_size);
    });
    server->on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "index.js", "text/javascript", true, index_js, index_js_size);
    });

    // System Info
    server->on("/systeminfo", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            char response_body[300];
            uint64_t LittleFSTotalBytes = LittleFS.totalBytes();
            uint64_t LittleFSUsedBytes = LittleFS.usedBytes();
            uint64_t SDTotalBytes = SD.totalBytes();
            uint64_t SDUsedBytes = SD.usedBytes();
            snprintf(
                response_body,
                sizeof(response_body),
                "{\"%s\":\"%s\",\"%s\":\"%s\",\"SD\":{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"},"
                "\"LittleFS\":{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}}",
                "BRUCE_VERSION",
                BRUCE_VERSION,
                "MALIOS_VERSION",
                MALIOS_VERSION,
                "free",
                humanReadableSize(SDTotalBytes - SDUsedBytes).c_str(),
                "used",
                humanReadableSize(SDUsedBytes).c_str(),
                "total",
                humanReadableSize(SDTotalBytes).c_str(),
                "free",
                humanReadableSize(LittleFSTotalBytes - LittleFSUsedBytes).c_str(),
                "used",
                humanReadableSize(LittleFSUsedBytes).c_str(),
                "total",
                humanReadableSize(LittleFSTotalBytes).c_str()
            );
            request->send(200, "application/json", response_body);
        }
    });

    // Get Screen
    server->on("/getscreen", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            static uint8_t *screenBinBuffer = nullptr;
            static size_t screenBinBufferSize = 0;

            if (!screenBinBuffer) {
                size_t desiredSize = MAX_LOG_ENTRIES * MAX_LOG_SIZE;
                if (psramFound()) screenBinBuffer = static_cast<uint8_t *>(ps_malloc(desiredSize));
                if (!screenBinBuffer) screenBinBuffer = static_cast<uint8_t *>(malloc(desiredSize));
                if (!screenBinBuffer) {
                    request->send(503, "text/plain", "Insufficient memory for screen buffer");
                    return;
                }
                screenBinBufferSize = desiredSize;
            }

            size_t binSize = 0;
            tft.getBinLog(screenBinBuffer, binSize);
            if (binSize > screenBinBufferSize) {
                request->send(500, "text/plain", "Screen buffer overflow");
                return;
            }
            request->send(200, "application/octet-stream", (const uint8_t *)screenBinBuffer, binSize);
        }
    });

    // Rename file or folder
    server->on("/rename", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) return;
        if (!request->hasArg("fs") || !request->hasArg("fileName") ||
            !request->hasArg("filePath")) {
            request->send(400, "text/plain", "ERROR: fs, fileName and filePath are required");
            return;
        }
        FS *fs = selectWebFileSystem(request->arg("fs"));
        const String fileName = request->arg("fileName");
        const String filePath = request->arg("filePath");
        if (!fs || !isSafeWebFilename(fileName) || !isSafeWebPath(filePath, true) ||
            filePath == "/") {
            request->send(400, "text/plain", "Invalid file system or path");
            return;
        }
        const String destination =
            filePath.substring(0, filePath.lastIndexOf('/') + 1) + fileName;
        if (!isSafeWebPath(destination, true)) {
            request->send(400, "text/plain", "Invalid destination path");
            return;
        }
        if (fs->rename(filePath, destination))
            request->send(200, "text/plain", filePath + " renamed to " + destination);
        else request->send(500, "text/plain", "Fail renaming file.");
    });

    // Route to send a generic command (Tasmota compatible API)
    // https://tasmota.github.io/docs/Commands/#with-web-requests
    server->on("/cm", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) { return; }
        if (request->hasArg("cmnd")) {
            String cmnd = request->arg("cmnd");
            if (cmnd.startsWith("nav")) {
                volatile bool *var = &SelPress;
                if (cmnd.startsWith("nav sel")) var = &SelPress;
                if (cmnd.startsWith("nav esc")) var = &EscPress;
                if (cmnd.startsWith("nav up")) var = &UpPress;
                if (cmnd.startsWith("nav down")) var = &DownPress;
                if (cmnd.startsWith("nav next")) var = &NextPress;
                if (cmnd.startsWith("nav prev")) var = &PrevPress;
                request->send(200, "text/plain", "command " + cmnd + " success");
                int time;
                if (cmnd.endsWith("0")) time = cmnd.substring(cmnd.lastIndexOf(' ')).toInt();
                else time = 10;
                auto tmp = millis() + time;
                while (tmp > millis()) {
                    AnyKeyPress = true;
                    SerialCmdPress = true;
                    *var = true;
                    if (!LongPress) vTaskDelay(pdMS_TO_TICKS(190));
                    else vTaskDelay(pdMS_TO_TICKS(50));
                }
            } else {
                if (parseSerialCommand(cmnd, false)) {
                    request->send(200, "text/plain", "command " + cmnd + " queued");
                } else {
                    request->send(400, "text/plain", "command failed, check the serial log for details");
                }
            }
        } else {
            request->send(400, "text/plain", "http request missing required arg: cmnd");
        }
    });

    // Reboot device
    server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) { ESP.restart(); }
    });

    // List files
    server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) return;
        const String folder = request->hasArg("folder") ? request->arg("folder") : "/";
        FS *fs = request->hasArg("fs") ? selectWebFileSystem(request->arg("fs")) : nullptr;
        if (!fs || !isSafeWebPath(folder, true)) {
            request->send(400, "text/plain", "Invalid file system or folder");
            return;
        }
        if (!fs->exists(folder)) {
            request->send(404, "text/plain", "Folder not found");
            return;
        }
        request->send(200, "text/plain", listFiles(*fs, folder));
    });

    // Download, create folder and delete
    server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) return;
        if (request->hasArg("name") && request->hasArg("action") && request->hasArg("fs")) {
                String fileName = request->arg("name");
                String fileAction = request->arg("action");
                FS *fs = selectWebFileSystem(request->arg("fs"));
                const bool validAction = fileAction == "download" || fileAction == "image" ||
                                         fileAction == "delete" || fileAction == "create" ||
                                         fileAction == "createfile" || fileAction == "edit";
                if (!fs || !isSafeWebPath(fileName, true) || !validAction ||
                    (fileName == "/" && fileAction == "delete")) {
                    request->send(400, "text/plain", "Invalid file system, path or action");
                    return;
                }

                log_i("filename: %s\n", fileName.c_str());
                log_i("fileAction: %s\n", fileAction.c_str());

                if (!fs->exists(fileName)) {
                    if (strcmp(fileAction.c_str(), "create") == 0) {
                        if (fs->mkdir(fileName)) {
                            request->send(200, "text/plain", "Created new folder: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating folder: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "createfile") == 0) {
                        File newFile = fs->open(fileName, FILE_WRITE, true);
                        if (newFile) {
                            newFile.close();
                            request->send(200, "text/plain", "Created new file: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating file: " + String(fileName));
                        }
                    } else request->send(400, "text/plain", "ERROR: file does not exist");

                } else {
                    if (strcmp(fileAction.c_str(), "download") == 0) {
                        request->send(*fs, fileName, "application/octet-stream", true);
                    } else if (strcmp(fileAction.c_str(), "image") == 0) {
                        String extension = fileName.substring(fileName.lastIndexOf('.') + 1);
                        // https://www.iana.org/assignments/media-types/media-types.xhtml#image
                        if (extension == "jpg") extension = "jpeg"; // www.rfc-editor.org/rfc/rfc2046.html
                        request->send(*fs, fileName, "image/" + extension);
                    } else if (strcmp(fileAction.c_str(), "delete") == 0) {
                        if (deleteFromSd(*fs, fileName)) {
                            request->send(200, "text/plain", "Deleted : " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL deleting: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "create") == 0) {
                        if (fs->mkdir(fileName)) {
                            request->send(200, "text/plain", "Created new folder: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating folder: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "createfile") == 0) {
                        File newFile = fs->open(fileName, FILE_WRITE, true);
                        if (newFile) {
                            newFile.close();
                            request->send(200, "text/plain", "Created new file: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating file: " + String(fileName));
                        }

                    } else if (strcmp(fileAction.c_str(), "edit") == 0) {
                        File editFile = fs->open(fileName, FILE_READ);
                        if (editFile) {
                            if (editFile.size() > WEB_EDITOR_MAX_BYTES) {
                                editFile.close();
                                request->send(413, "text/plain", "File is too large for the web editor");
                                return;
                            }
                            String fileContent = editFile.readString();
                            request->send(200, "text/plain", fileContent);
                            editFile.close();
                        } else {
                            request->send(500, "text/plain", "Failed to open file for reading");
                        }

                    } else {
                        request->send(400, "text/plain", "ERROR: invalid action param supplied");
                    }
                }
        } else {
            request->send(400, "text/plain", "ERROR: fs, name and action params required");
        }
    });

    // Edit file
    server->on("/edit", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) return;
        if (request->contentLength() > WEB_EDITOR_MAX_BYTES + 8192) {
            request->send(413, "text/plain", "Edit request is too large");
            return;
        }
        if (request->hasArg("name") && request->hasArg("content") && request->hasArg("fs")) {
                String fileName = request->arg("name");
                String fileContent = request->arg("content");
                FS *fs = selectWebFileSystem(request->arg("fs"));
                if (!fs || !isSafeWebPath(fileName, true) ||
                    fileContent.length() > WEB_EDITOR_MAX_BYTES) {
                    request->send(400, "text/plain", "Invalid file system, path or content size");
                    return;
                }

                File editFile = fs->open(fileName, FILE_WRITE);
                if (editFile) {
                    if (editFile.write((const uint8_t *)fileContent.c_str(), fileContent.length())) {
                        request->send(200, "text/plain", "File edited: " + fileName);
                    } else {
                        request->send(500, "text/plain", "Failed to write to file: " + fileName);
                    }
                    editFile.close();
                } else {
                    request->send(500, "text/plain", "Failed to open file for writing: " + fileName);
                }

        } else {
            request->send(400, "text/plain", "ERROR: name, content, and fs parameters required");
        }
    });

    // File upload
    server->on(
        "/upload",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!checkUserWebAuth(request)) return;
            WebUploadState *state = static_cast<WebUploadState *>(request->_tempObject);
            if (!state) {
                request->send(400, "text/plain", "Invalid upload request");
                return;
            }
            const uint16_t status = state->failed ? state->status : 200;
            const String message = state->failed ? state->message : "File upload completed";
            free(state);
            request->_tempObject = nullptr;
            request->send(status, "text/plain", message);
        },
        handleUpload
    );

    // Wi-Fi configuration
    server->on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            if (request->contentLength() > 256 || !request->hasParam("usr", true) ||
                !request->hasParam("pwd", true)) {
                request->send(400, "text/plain; charset=utf-8", "Credenciais ausentes ou invalidas");
                return;
            }
            const String &usr = request->getParam("usr", true)->value();
            const String &pwd = request->getParam("pwd", true)->value();
            if (usr.isEmpty() || usr.length() > 32 || pwd.isEmpty() || pwd.length() > 64) {
                request->send(400, "text/plain; charset=utf-8", "Credenciais fora dos limites");
                return;
            }
            bruceConfig.setWebUICreds(usr.c_str(), pwd.c_str());
            request->send(200, "text/plain; charset=utf-8", "Credenciais do WebUI atualizadas");
        }
    });
    server->begin();
    Serial.println("Webserver started");
}

/**********************************************************************
**  Function: startWebUi
**  Start the WebUI
**********************************************************************/
void startWebUi(bool mode_ap) {
    bool keepWifiConnected = false;
    if (!WiFi.isConnected()) {
        if (mode_ap) wifiConnectMenu(WIFI_AP);
        else wifiConnectMenu(WIFI_STA);
    } else {
        keepWifiConnected = true;
    }

    // configure web server

    if (!server) {
        // Clear this vector to free stack memory
        options.clear();

        Serial.println("Configuring Webserver ...");
        if (psramFound()) server = (AsyncWebServer *)ps_malloc(sizeof(AsyncWebServer));
        else server = (AsyncWebServer *)malloc(sizeof(AsyncWebServer));

        new (server) AsyncWebServer(default_webserverporthttp);

        configureWebServer();

        isWebUIActive = true;
    }
    tft.setLogging();
    drawWebUiScreen(mode_ap);
#ifdef HAS_SCREEN // Headless always run in the background!
    while (!check(EscPress)) {
        if (CounterLab::serviceDisplay()) drawWebUiScreen(mode_ap);
        MaliWifiWebApi::service();
        // Consume TFT work in this foreground UI task, never in AsyncTCP.
        if (MaliQrService::processPendingDisplay()) drawWebUiScreen(mode_ap);
        KeyGauge::processWebPreview();
        vTaskDelay(pdMS_TO_TICKS(70));
    }

    bool closeServer = false;

    options.clear();
    options.emplace_back("Executar em segundo plano", []() {});
    options.emplace_back("Sair", [&closeServer]() { closeServer = true; });

    loopOptions(options);

    if (closeServer) {
        stopWebUi();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!keepWifiConnected) { wifiDisconnect(); }
    }
#endif
}
