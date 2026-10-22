#include <Arduino.h>
#include "Arducam_Mega.h"
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

// Webサーバーのインスタンス化
AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager wifiManager(&server, &dns);

CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SXGAM; // =5, 1024x768
const int CS = 1; //for ESP32 CS=17, ESPC3 CS=1
Arducam_Mega myCAM(CS);
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;
const size_t bufferSize = 4096; // Increased buffer size from 2048;
uint8_t buffer[bufferSize] = {0};

void sendImageData(AsyncWebServerRequest *request)
{
    int i = 0;
    AsyncClient* client = request->client();
    if (!client->connected()) return;
    while (myCAM.getReceivedLength()) {
        imageData = imageDataNext;
        imageDataNext = myCAM.readByte();
        if (headFlag == 1) {
            buffer[i++] = imageDataNext;
            if (i >= bufferSize)  {
                if (!client->connected()) break;
                client->write(reinterpret_cast<const char*>(buffer), i);
                i = 0;
            }
        }
        if (imageData == 0xff && imageDataNext == 0xd8) {
            headFlag = 1;
            buffer[i++] = imageData;
            buffer[i++] = imageDataNext;
        }
        if (imageData == 0xff && imageDataNext == 0xd9) {
            headFlag = 0;
            if (!client->connected()) break;
            client->write(reinterpret_cast<const char*>(buffer), i);
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

void handleSetBrightness(AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
        String value = request->getParam("value")->value();
        if (value == "HIGH") {
            setBrightness(CAM_BRIGHTNESS_LEVEL_MINUS_4);
        } else if (value == "DEFAULT") {
            setBrightness(CAM_BRIGHTNESS_LEVEL_DEFAULT);
        } else if (value == "LOW") {
            setBrightness(CAM_BRIGHTNESS_LEVEL_2);
        }
        request->send(200, "text/plain", "Brightness set to " + value);
    } else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void captureCallbackFunction(AsyncWebServerRequest *request)
{
    myCAM.takePicture(imageMode, CAM_IMAGE_PIX_FMT_JPG);
    AsyncClient* client = request->client();
    if (!client->connected()) return;
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: image/jpeg\r\n";
    response += "Content-len: " + String(7000) + "\r\n\r\n";
    sendImageData(request);
}

void streamCallbackFunction(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", "Streaming...");
    Serial.println("Entered streamCallbackFunction");
    AsyncClient* client = request->client();
    static unsigned long lastCaptureTime = 0;
    const unsigned long captureInterval = 10000; // Capture every 10 seconds
    while (client->connected())
    {
        if (millis() - lastCaptureTime >= captureInterval)
        {
            captureCallbackFunction(request);
            lastCaptureTime = millis();
        }
    }
}

void handleNotFound(AsyncWebServerRequest *request)
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += request->args();
    message += "\n";
    request->send(200, "text/plain", message);
    Serial.println(message);
    if (request->hasArg("ql"))
    {
        imageMode = (CAM_IMAGE_MODE)request->arg("ql").toInt();
    }
}

void setup() {
    Serial.begin(115200);
    // WiFi接続の初期化
    wifiManager.autoConnect("AutoConnectAP");

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
    // Webサーバーのエンドポイント設定
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
            <!DOCTYPE html>
            <html lang="ja">
            <head>
            <meta charset="UTF-8">
            <title>カメラ制御</title>
            <style>button { font-size: 24px; padding: 10px 20px; margin: 10px; }</style>
            </head>
            <body>
            <img id="cameraCapture" src="/capture" />
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
            <button onclick="sendRequest('/setBrightness?value=HIGH')">明るく</button>
            <button onclick="sendRequest('/setBrightness?value=DEFAULT')">普通</button>
            <button onclick="sendRequest('/setBrightness?value=LOW')">暗く</button>
            </body>
            </html>
        )rawliteral");
        // server.sendContent(html);
        // server.send(200, "text/html", html);
        });
    // server.enableCORS(); // Removed as AsyncWebServer does not have this member function
    server.on("/capture", HTTP_GET, captureCallbackFunction);
    server.on("/stream", HTTP_GET, streamCallbackFunction);
    server.on("/setBrightness", HTTP_GET, handleSetBrightness);
    // server.on("/HIGH", []() { 
    //     handleSetBrightness(CAM_BRIGHTNESS_LEVEL_4); 
    //     server.send(200, "text/plain", "Brightness set to HIGH"); });
    // server.on("/DEFAULT", []() { handleSetBrightness(CAM_BRIGHTNESS_LEVEL_DEFAULT); server.send(200, "text/plain", "Brightness set to DEFAULT"); });
    // server.on("/LOW", []() { handleSetBrightness(CAM_BRIGHTNESS_LEVEL_MINUS_4); server.send(200, "text/plain", "Brightness set to LOW"); });
    server.onNotFound(handleNotFound);
    server.begin();
}

void loop() {
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 10000) {
        Serial.print("Free heap in loop: ");
        Serial.println(ESP.getFreeHeap());
        lastPrintTime = millis();
    }
}

