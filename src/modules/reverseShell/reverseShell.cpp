#include "core/ui/MaliUI.h"
#if !defined(LITE_VERSION)
#include "core/display.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

void ReverseShell() {
    AsyncWebServer webServer(80);
    AsyncWebSocket ws("/ws");
    DNSServer dnsServer;
    IPAddress apGateway(192, 168, 4, 1);
    WiFiServer tcpServer(23);
    WiFiClient tcpClient;
    bool shellConnected = false;
    bool wsConnected = false;

    // ── WebSocket Event Handler ────────────────────────────────
    auto onWsEvent = [&](AsyncWebSocket *server, AsyncWebSocketClient *client,
                         AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                wsConnected = true;
                client->text("Conectado ao BruceShell!\r\n");
                break;

            case WS_EVT_DISCONNECT:
                wsConnected = false;
                break;

            case WS_EVT_DATA:
                if (shellConnected && tcpClient) {
                    String cmd = String((char*)data);
                    cmd.trim();
                    if (cmd.length() > 0) {
                        tcpClient.println(cmd);

                        // Read output and send back via WebSocket
                        String output = "";
                        unsigned long timeout = millis() + 3000;
                        while (millis() < timeout) {
                            if (tcpClient.available()) {
                                output += tcpClient.readString();
                            }
                            if (output.endsWith("\n> ") || output.endsWith("\n$ ") || output.endsWith("\n# ")) {
                                break;
                            }
                            delay(10);
                        }
                        if (output.length() > 0) {
                            client->text(output);
                        } else {
                            client->text("[Comando executado, sem saida]\r\n");
                        }
                    }
                } else {
                    client->text("Erro: nenhum shell conectado.\r\n");
                }
                break;

            case WS_EVT_ERROR:
                // Handle error silently
                break;
        }
    };

    // ── Setup ──────────────────────────────────────────────────
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextSize(FM);
    tft.setTextColor(MaliUI::ERROR, bruceConfig.bgColor);
    tft.drawCentreString("Shell reverso", tftWidth / 2, 10, 1);
    tft.setTextColor(MaliUI::TEXT_PRIMARY, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(15, 33);
    tft.println("Por Fourier & Ninja-jr");
    tft.println("Iniciando servidor...");

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(apGateway, apGateway, IPAddress(255, 255, 255, 0))) {
        tft.println("Falha ao configurar AP");
        return;
    }

    // ── AP Password: bruce ─────────────────────────────────────
    if (!WiFi.softAP("BruceShell", "bruce")) {
        tft.println("Falha ao iniciar AP");
        return;
    }

    tft.println("AP Wi-Fi ativo: BruceShell (senha: bruce)");
    tft.println("IP: " + apGateway.toString());

    tcpServer.begin();
    tft.println("Servidor TCP ativo na porta 23.");

    // ── Web Interface ──────────────────────────────────────────
    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
            <!DOCTYPE html>
            <html lang="pt-BR">
            <head>
                <title>BruceShell</title>
                <style>
                    body { background: #0a0a1a; color: #00ff41; font-family: 'Courier New', monospace; margin: 0; padding: 20px; }
                    .container { max-width: 800px; margin: auto; }
                    .status { display: inline-block; width: 12px; height: 12px; border-radius: 50%; }
                    .online { background: #00ff41; }
                    .offline { background: #ff0040; }
                    input { width: 70%; padding: 10px; background: #16213e; color: #00ff41; border: 1px solid #00ff41; }
                    button { padding: 10px 20px; background: #16213e; color: #00ff41; border: 1px solid #00ff41; cursor: pointer; }
                    button:hover { background: #1a2a4e; }
                    #output { background: #0a0a1a; padding: 10px; min-height: 300px; max-height: 400px; white-space: pre-wrap; border: 1px solid #00ff41; margin-top: 10px; overflow-y: auto; }
                    .footer { margin-top: 20px; font-size: 12px; color: #666; }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>🔴 BruceShell <span id="statusDot" class="status offline"></span> <span id="statusText">Desconectado</span></h1>
                    <p>IP: 192.168.4.1 | Porta: 23</p>
                    <div style="display: flex; gap: 10px; flex-wrap: wrap;">
                        <input type="text" id="cmd" placeholder="Digite o comando..." onkeyup="if(event.keyCode==13) sendCommand();" autofocus>
                        <button onclick="sendCommand();">Executar</button>
                        <button onclick="clearOutput();">Limpar</button>
                    </div>
                    <div id="output">~ BruceShell\n~ Conectado: aguardando shell...</div>
                    <div class="footer">Conectar por WebSocket ws://192.168.4.1/ws</div>
                </div>
                <script>
                    var ws = new WebSocket('ws://192.168.4.1/ws');
                    ws.onopen = function() {
                        document.getElementById('statusDot').className = 'status online';
                        document.getElementById('statusText').innerText = 'Conectado';
                    };
                    ws.onclose = function() {
                        document.getElementById('statusDot').className = 'status offline';
                        document.getElementById('statusText').innerText = 'Desconectado';
                    };
                    ws.onmessage = function(e) {
                        document.getElementById('output').innerText += e.data;
                        document.getElementById('output').scrollTop = document.getElementById('output').scrollHeight;
                    };
                    function sendCommand() {
                        var cmd = document.getElementById('cmd').value;
                        if (cmd) {
                            ws.send(cmd + '\n');
                            document.getElementById('cmd').value = '';
                            document.getElementById('cmd').focus();
                        }
                    }
                    function clearOutput() {
                        document.getElementById('output').innerText = '';
                    }
                </script>
            </body>
            </html>
        )rawliteral";
        request->send(200, "text/html", html);
    });

    webServer.begin();
    tft.println("Servidor web ativo na porta 80!");
    tft.println("Servidor WebSocket ativo em /ws");

    dnsServer.start(53, "*", apGateway);

    // ── Main Loop ──────────────────────────────────────────────
    while (true) {
        dnsServer.processNextRequest();
        ws.cleanupClients();

        if (!shellConnected) {
            tcpClient = tcpServer.accept();
            if (tcpClient) {
                tft.println("Cliente conectado.");
                tcpClient.println("~Welcome to BruceShell.");
                tcpClient.println("~Developed by Fourier & Ninja-jr");
                tcpClient.println("~Type 'help' for available commands");
                shellConnected = true;
            }
        }

        if (shellConnected && !tcpClient.connected()) {
            tft.println("Cliente desconectado.");
            shellConnected = false;
            tcpClient.stop();
        }

        if (check(EscPress)) {
            tft.println("Saindo do servidor...");
            tcpServer.stop();
            ws.closeAll();
            webServer.end();
            dnsServer.stop();
            break;
        }
        delay(10);
    }
}
#endif
