// TODO: Be able to read bytes from server in background/task
//       so there is no loss of data when inputing
#ifndef LITE_VERSION
#include "modules/wifi/tcp_utils.h"
#include "core/display.h"
#include "core/wifi/wifi_common.h"

bool inputMode;

void listenTcpPort() {
    if (!wifiConnected) wifiConnectMenu();

    String portNumber = num_keyboard("", 5, "Porta TCP para escutar");
    if (portNumber.length() == 0 || portNumber == "\x1B") {
        displayError("Porta nao informada. Saindo");
        return;
    }
    int portNumberInt = atoi(portNumber.c_str());
    if (portNumberInt == 0) {
        displayError("Porta invalida. Saindo");
        return;
    }

    WiFiClient tcpClient;
    drawMainBorderWithTitle("ESCUTA TCP");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    WiFiServer server(portNumberInt);
    server.begin();

    padprintln("");
    padprint("Escutando em:");
    tft.print(WiFi.localIP().toString().c_str());
    tft.println(":" + portNumber);

    for (;;) {
        WiFiClient client = server.accept();

        if (client) {
            Serial.println("Client connected");
            tft.setCursor(10, tft.getCursorY());
            tft.println("Cliente conectado");

            while (client.connected()) {
                if (inputMode) {
                    String keyString = keyboard("", 16, "Enviar dados, q=sair");
                    if (keyString == "q" || keyString == "\x1B") {
                        displayError("Saindo da escuta");
                        client.stop();
                        server.stop();
                        return;
                    }
                    inputMode = false;
                    drawMainBorderWithTitle("ESCUTA TCP");
                    tft.setTextSize(FP);
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                    tft.setCursor(10, BORDER_PAD_Y + FM * LH);
                    if (keyString.length() > 0 && keyString != "\x1B") {
                        if (tft.getCursorY() > tftHeight - 3 * LH * FP) {
                            drawMainBorderWithTitle("ESCUTA TCP");
                            padprint(keyString);
                        }
                        client.print(keyString);
                        Serial.print(keyString);
                    }
                } else {
                    if (client.available()) {
                        String incomingData = client.readString();
                        if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("ESCUTA TCP");
                        padprint(incomingData);
                        Serial.print(incomingData);
                    }
                    if (check(SelPress)) { inputMode = true; }
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            client.stop();
            Serial.println("Client disconnected");
            displayError("Cliente desconectado");
        }
        if (check(EscPress)) {
            displayError("Saindo da escuta");
            server.stop();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void clientTCP() {
    if (!wifiConnected) wifiConnectMenu();

    String _ip = WiFi.localIP().toString();
    _ip = _ip.substring(0, _ip.lastIndexOf('.')) + ".";

    String serverIP = num_keyboard(_ip, 15, "IP do servidor");
    if (serverIP == "\x1B") return;
    String portString = num_keyboard("", 5, "Porta do servidor");
    if (portString == "\x1B") return;
    int portNumber = atoi(portString.c_str());

    if (serverIP.length() == 0 || portNumber == 0) {
        displayError("IP ou porta invalida");
        return;
    }

    WiFiClient client;
    drawMainBorderWithTitle("CLIENTE TCP");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    padprintln("Conectando a:");
    tft.println(serverIP + ":" + portString);

    if (!client.connect(serverIP.c_str(), portNumber)) {
        displayError("Falha na conexao");
        return;
    }

    padprintln("Conectado!");
    Serial.println("Connected to server");

    while (client.connected()) {
        if (inputMode) {
            String keyString = keyboard("", 16, "Enviar dados");
            inputMode = false;
            drawMainBorderWithTitle("CLIENTE TCP");
            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(10, BORDER_PAD_Y + FM * LH);
            if (keyString.length() > 0 && keyString != "\x1B") {
                if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("ESCUTA TCP");
                padprint(keyString);
                client.print(keyString);
                Serial.print(keyString);
            }
        } else {
            if (client.available()) {
                String incomingData = client.readString();
                if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("ESCUTA TCP");
                padprint(incomingData);
                Serial.print(incomingData);
            }
            if (check(SelPress)) { inputMode = true; }
        }
        if (check(EscPress)) {
            displayError("Saindo do cliente");
            client.stop();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    displayError("Conexao encerrada.");
    Serial.println("Connection closed.");
    client.stop();
}
#endif
