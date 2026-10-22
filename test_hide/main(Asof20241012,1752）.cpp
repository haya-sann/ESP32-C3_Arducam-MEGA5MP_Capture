#include <Arduino.h>
#include <WiFi.h>
#include "Arducam_Mega.h"
#include <WebServer.h>
#include <WiFiManager.h>

WebServer server(80);
WiFiManager wifiManager;

CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SXGAM; // =5, 1024x768
const int CS = 1; //for ESP32 CS=17, ESPC3 CS=1
Arducam_Mega myCAM(CS);
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;
const size_t bufferSize = 16384; // Increased buffer size from 2048;
uint8_t buffer[bufferSize] = {0};

void sendImageData(void)
{
    int i = 0;
    WiFiClient client = server.client();
    if (!client.connected()) return;
    while (myCAM.getReceivedLength()) {
        imageData = imageDataNext;
        imageDataNext = myCAM.readByte();
        if (headFlag == 1)  {
            buffer[i++] = imageDataNext;
            if (i >= bufferSize)  {
                if (!client.connected()) break;
                client.write(buffer, i);
                i = 0;
            }
        }
        if (imageData == 0xff && imageDataNext == 0xd8) {
            headFlag = 1;
            buffer[i++] = imageData;
            buffer[i++] = imageDataNext;
        }
        if (imageData == 0xff && imageDataNext == 0xd9)  {
            headFlag = 0;
            if (!client.connected()) break;
            client.write(buffer, i);
            i = 0;
            break;
        }
    }
}

void setBrightness(CAM_BRIGHTNESS_LEVEL brightnessValue)
{
    Serial.print("Setting brightness(For debugging):");
    Serial.println(brightnessValue);
    myCAM.setBrightness(brightnessValue); // Configures the brightness of the image
}

void handleSetBrightness(CAM_BRIGHTNESS_LEVEL brightnessValue) {
    Serial.println("Entered handle setBrightness(For debugging):");
    if (server.hasArg("value")) {
        int brightnessValue = server.arg("value").toInt();
        Serial.print("handle setBrightness(For debugging):");
        Serial.println(brightnessValue);
        setBrightness(static_cast<CAM_BRIGHTNESS_LEVEL>(brightnessValue));
        server.send(200, "text/plain", String(brightnessValue));
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}

void captureCallbackFunction(void)
{
    myCAM.takePicture(imageMode, CAM_IMAGE_PIX_FMT_JPG);
    WiFiClient client = server.client();
    if (!client.connected()) return;
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: image/jpeg\r\n";
    response += "Content-len: " + String(7000) + "\r\n\r\n";
    sendImageData();
}

void streamCallbackFunction(void) {
    Serial.println("Entered streamCallbackFunction");
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (client.connected()) {
        myCAM.takePicture(imageMode, CAM_IMAGE_PIX_FMT_JPG);
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);
        sendImageData();
        server.sendContent("\r\n");
        delay(100); // Add a delay to prevent the server from being overwhelmed
    }
}

void handleNotFound(void)
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
    Serial.println(message);
    if (server.hasArg("ql"))
    {
        imageMode = (CAM_IMAGE_MODE)server.arg("ql").toInt();
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConfigPortalTimeout(60);

    if (wifiManager.autoConnect("AutoConnectAP")) {
        Serial.println("connected...yeey :)");
    } else {
        Serial.println("Configportal running");
    }

    delay(10000); // Add a 10-second delay
    Serial.println("Connected to WiFi");
    Serial.print("Server IP Address: ");
    Serial.println(WiFi.localIP());

    myCAM.begin();
    myCAM.setAutoFocus(1);

    server.on("/", HTTP_GET, []() {
        String html = "HTTP/1.1 200 OK\r\n";
        html += "Content-Type: text/html\r\n\r\n";
        html += R"rawliteral(
            <!DOCTYPE html>
            <html lang="ja">
            <head>
            <meta charset="UTF-8">
            <title>カメラ制御</title>
            <style>button { font-size: 24px; padding: 10px 20px; margin: 10px; }</style>
            </head>
            <body>
            <img id="cameraStream" src="/stream" />
            <script>
            function sendRequest(url) {
              var xhr = new XMLHttpRequest();
              xhr.open('GET', url, true);
              xhr.onreadystatechange = function () {
            if (xhr.readyState == 4 && xhr.status == 200) {
              console.log('Response:', xhr.responseText);
            }
              };
              xhr.send();
            }
            </script>
            <h1>ESP32-C3 LED Control</h1>
            <button onclick="sendRequest('/HIGH')">明るく</button>
            <button onclick="sendRequest('/DEFAULT')">通常</button>
            <button onclick="sendRequest('/LOW')">暗く</button>
            <button onclick="sendRequest('/capture')">撮影</button>
            </body>
            </html>
        )rawliteral";
        server.sendContent(html);
        // server.send(200, "text/html", html);
        });
    server.enableCORS();
    server.on("/capture", captureCallbackFunction);
    server.on("/stream", streamCallbackFunction);
    // server.on("/setBrightness", handleSetBrightness);
    server.on("/HIGH", []() { 
        handleSetBrightness(CAM_BRIGHTNESS_LEVEL_1); 
        server.send(200, "text/plain", "Brightness set to HIGH"); });
    server.on("/DEFAULT", []() { handleSetBrightness(CAM_BRIGHTNESS_LEVEL_DEFAULT); server.send(200, "text/plain", "Brightness set to DEFAULT"); });
    server.on("/LOW", []() { handleSetBrightness(CAM_BRIGHTNESS_LEVEL_2); server.send(200, "text/plain", "Brightness set to LOW"); });
    server.onNotFound(handleNotFound);

    // Print free heap memory
    Serial.print("Free heap at setup: ");
    Serial.println(ESP.getFreeHeap());

    server.begin();
}

void loop() {
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 10000) {
        Serial.print("Free heap in loop: ");
        Serial.println(ESP.getFreeHeap());
        lastPrintTime = millis();
    }
    server.handleClient(); 
}

