

#include <Arduino.h>
/*
// Copyright 2021 Arducam Technology co., Ltd. All Rights Reserved.
// License: MIT License (https://en.wikipedia.org/wiki/MIT_License)
// Web: http://www.ArduCAM.com
API Reference at : https://www.arducam.com/docs/arducam-mega/arducam-mega-getting-started/packs/C%2B%2B_ApiDoc.html
View at : http://192.168.0.39
*/
#include <WiFi.h>
#include <WebServer.h>
#include "Arducam_Mega.h"
// #include "Platform.h"

#include <WiFiManager.h> // Include the WiFiManager library
WiFiManager wifiManager;

// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_QVGA; // =1 ,320x240
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_VGA; // =2 , 640x480
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SVGA; // =3, 800x600
CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_HD; // =4, 1280x720
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SXGAM; // =5, 1024x768
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_UXGA; // =6, 1600x1200
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_FHD; // =7, 1920x1080
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_QXGA; // =8, 2048x1536 
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_WQXGA2; // =9 , 2592x1944
WebServer server(80);
const int CS = 1; //for ESP32 CS=17, ESPC3 CS=1
Arducam_Mega myCAM( CS );
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;
// const size_t bufferSize = 2048; // Increased buffer size from 2048;
const size_t bufferSize = 16384; // Increased buffer size from 2048;
// const size_t bufferSize = 32768; // Increased buffer size from 2048;
// const size_t bufferSize = 65536; // Increased buffer size from 2048;
uint8_t buffer[bufferSize] = {0};

void sendImageData(void)
{
    int i = 0;
    WiFiClient client = server.client();
    if (!client.connected()) return;
    while (myCAM.getReceivedLength())
    {
        imageData = imageDataNext;
        imageDataNext = myCAM.readByte();
        if (headFlag == 1)
        {
            buffer[i++]=imageDataNext;  
            if (i >= bufferSize)
            {
                if (!client.connected()) break;
                client.write(buffer, i);
                i = 0;
            }
        }
        if (imageData == 0xff && imageDataNext ==0xd8)
        {
            headFlag = 1;
            buffer[i++]=imageData;
            buffer[i++]=imageDataNext;  
        }
        if (imageData == 0xff && imageDataNext ==0xd9)
        {
            headFlag = 0;
            if (!client.connected()) break;
            client.write(buffer, i);
            i = 0;
            break;
        }
    }
}
void handleRoot() { 
    WiFiClient client = server.client();
    
    // HTMLとJavaScriptの送信
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html\r\n\r\n"; // Content-Typeをtext/htmlに設定
    response += R"(
        <!DOCTYPE html>
        <html lang="ja">
        <head>
            <meta charset="UTF-8">
            <title>カメラ制御</title>
        </head>
        <style>button { font-size: 24px; padding: 10px 20px; margin: 10px; }</style>
        <body>
            <img id="cameraStream" src="/stream" />
            <button id="brightButton">明るく</button>
            <button id="defaultButton">標準</button>
            <button id="darkButton">暗く</button>
            <script>
                // CAM_BRIGHTNESS_LEVELオブジェクトを定義
                //     const CAM_BRIGHTNESS_LEVEL = {
                //     HIGH: 4,
                //     DEFAULT: 0,
                //     LOW: 2
                // };
                // ボタンがクリックされたときの処理を定義
                document.getElementById('brightButton').addEventListener('click', function() {
                    fetch('/setBrightness?value=4')
                        .then(response => {
                                if (!response.ok) {
                                    throw new Error('Network response was not ok');
                                }
                                return response.text();
                            })
                        .then(data => {
                        console.log('Response:', data);
                        alert(data);
                })
                        .catch(error => console.error('Error:', error));
                });
                document.getElementById('defaultButton').addEventListener('click', function() {
                    fetch('/setBrightness?value=0')
                        .then(response => response.text())
                        .then(data => alert(data))
                        .catch(error => console.error('Error:', error));
               });
                 document.getElementById('darkButton').addEventListener('click', function() {
                    fetch('/setBrightness?value=2')
                        .then(response => response.text())
                        .then(data => alert(data))
                        .catch(error => console.error('Error:', error));
                });
            </script>
        </body>
        </html>
    )";
    server.sendContent(response);
}

void setBrightness(CAM_BRIGHTNESS_LEVEL brightnessValue)
{
    Serial.print("Setting brightness(For debugging):");
    Serial.println(brightnessValue);
    // myCAM.setEV(CAM_EV_LEVEL_2);                 // Adjust the parameter as needed
    // myCAM.setBrightness(CAM_BRIGHTNESS_LEVEL_2); // Configures the brightness of the image
    // myCAM.setEV(brightnessValue);                 // Adjust the parameter as needed
    myCAM.setBrightness(brightnessValue); // Configures the brightness of the image
}

void handleSetBrightness() {
    Serial.println("Entered handle setBrightness(For debugging):");
    if (server.hasArg("value")) {
        int brightnessValue = server.arg("value").toInt();
        Serial.print("handle setBrightness(For debugging):");
        Serial.println(brightnessValue);
        setBrightness(static_cast<CAM_BRIGHTNESS_LEVEL>(brightnessValue));
        server.send(200, "text/plain", String(brightnessValue));
        Serial.println("Failed to send response");
    } else {
        server.send(400, "text/plain", "Bad Request");
        Serial.println("Failed to send response");
    }
}

void captureCallbackFunction(void)
{
    // Increase the exposure to make the image brighter
    myCAM.takePicture(imageMode,CAM_IMAGE_PIX_FMT_JPG);
    WiFiClient client = server.client();
    if (!client.connected()) return;
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: image/jpeg\r\n";
    response += "Content-len: " + String(7000) + "\r\n\r\n";  
    sendImageData();
}

void streamCallbackFunction(void){
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
    // response += "Content-Type: text/html\r\n\r\n"; // Content-Typeをtext/htmlに設定
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    // client.print(response);
    while(1) { // 画像データの送信ループ
    myCAM.takePicture(imageMode,CAM_IMAGE_PIX_FMT_JPG);
    if (!client.connected()) break; //ネットワーク接続が切れた場合にループを終了
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

//    server.sendContent(response);
    // client.print(response); 
    sendImageData();
    server.sendContent("\r\n");
    // client.print("\r\n");
    }
  }

void handleNotFound(void)
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
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

void setup(){
    Serial.begin(115200);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP    

    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConfigPortalTimeout(60); // Set timeout for the configuration portal
    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name

    // wifiManager.resetSettings();

    // Connect to WiFi using WiFiManager
   if(wifiManager.autoConnect("AutoConnectAP")){
        Serial.println("connected...yeey :)");
    }
    else {
        Serial.println("Configportal running");
    }
    // Print the assigned IP address
    Serial.println("Connected to WiFi");
    Serial.print("Server IP Address: ");
    Serial.println(WiFi.localIP());

    myCAM.begin();
    myCAM.setAutoFocus(1); // Set the focus mode, Auto focus is set by default
      // 明るさを設定 (例: 0から255の範囲で設定)
    // myCAM.setBrightness(CAM_BRIGHTNESS_LEVEL_4); // Configures the brightness of the image これはWebページから設定する

    server.enableCORS(); //CORS（Cross-Origin Resource Sharing）を有効にします。これにより、異なるオリジンからのリクエストが許可されます
    server.on("/",handleRoot);
    server.on("/capture",captureCallbackFunction);
    server.on("/stream",streamCallbackFunction);
    server.on("/setBrightness", handleSetBrightness);
    // server.on("/setBrightness", HTTP_GET, handleSetBrightness);
    server.onNotFound(handleNotFound);
    server.begin();

    // Print free heap memory
    Serial.print("Free heap at setup: ");
    Serial.println(ESP.getFreeHeap());
}

void loop(){
    // Print free heap memory periodically
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 10000) { // Print every 10 seconds 
        Serial.print("Free heap in loop: ");
        Serial.println(ESP.getFreeHeap());
        lastPrintTime = millis();
    }
    server.handleClient();
}
