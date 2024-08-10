#include <Arduino.h>

// Copyright 2021 Arducam Technology co., Ltd. All Rights Reserved.
// License: MIT License (https://en.wikipedia.org/wiki/MIT_License)
// Web: http://www.ArduCAM.com
#include <WiFi.h>
#include <WebServer.h>
#include "Arducam_Mega.h"
// #include "Platform.h"

#include <WiFiManager.h> // Include the WiFiManager library
WiFiManager wifiManager;

// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_QVGA; // =1 ,320x240
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_VGA; // =2 , 640x480
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SVGA; // =3, 800x600
CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_SXGAM; // =4, 1024x768
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_UXGA; // =5, 1600x1200
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_UXGA2; // =7, 1600x1600
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_QXGA; // =8, 2048x1536 
// CAM_IMAGE_MODE imageMode = CAM_IMAGE_MODE_WQXGA2; // =9 , 2592x1944
WebServer server(80);
const int CS = 1; //for ESP32 CS=17, ESPC3 CS=1
Arducam_Mega myCAM( CS );
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;
const size_t bufferSize = 32768; // Increased buffer size from 2048;
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

void streamCallbackFunction(void)
{
  WiFiClient client = server.client();
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  while(1)
  {
    myCAM.takePicture(imageMode,CAM_IMAGE_PIX_FMT_JPG);
    if (!client.connected()) break;
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response); 
    sendImageData();      
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
    myCAM.setAutoFocus(2);
    myCAM.setEV(CAM_EV_LEVEL_2); // Adjust the parameter as needed
    myCAM.setBrightness(CAM_BRIGHTNESS_LEVEL_2); //Configures the brightness of the image

    server.enableCORS();
    server.on("/capture",HTTP_GET,captureCallbackFunction);
    server.on("/stream",HTTP_GET,streamCallbackFunction);
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
