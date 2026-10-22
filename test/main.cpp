/*
AJAXを使ってESP32-C3のLEDを制御するサンプルプログラム,基本となるシンプルなコード
*/
#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>

const int LED_PIN = 9; // ESP32-C3に刺すLEDピン
bool isFlashing = false; // LEDが点滅中かどうかを示すフラグ
unsigned long flashInterval = 1000; // 点滅の間隔（ミリ秒）

AsyncWebServer server(80);

void ledFlash() {
    static unsigned long lastFlashTime = 0;
    unsigned long currentTime = millis();
    if (isFlashing && (currentTime - lastFlashTime >= flashInterval)) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // LEDの状態を反転
        lastFlashTime = currentTime;
    }
}

void setup() {
    // シリアル通信の初期化
    Serial.begin(115200);

    // LEDピンを出力モードに設定
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // 初期状態は消灯

    // WiFiManagerのインスタンスを作成
    WiFiManager wifiManager;

    // WiFi接続を開始
    if (!wifiManager.autoConnect("ESP32-C3_AP")) {
        Serial.println("Failed to connect and hit timeout");
        ESP.restart();
    }

    // WiFi接続が成功した場合
    delay(10000); // 10秒のウエイトを入れる
    Serial.println("Connected to WiFi");
    // IPアドレスを取得してシリアルモニタに表示
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // サーバーのルートURLにアクセスしたときの処理
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<html><head><meta charset=\"UTF-8\"><style>button { font-size: 32px; padding: 10px 20px; margin: 10px; }</style>"
        "<script>"
        "function sendRequest(url) {"
        "  var xhr = new XMLHttpRequest();"
        "  xhr.open('GET', url, true);"
        "  xhr.send();"
        "}"
        "</script></head>"
        "<body><h1>ESP32-C3 LED Control</h1>"
        "<button onclick=\"sendRequest('/on')\">点灯</button>"
        "<button onclick=\"sendRequest('/off')\">消灯</button>"
        "<button onclick=\"sendRequest('/flash')\">FLASH</button>"
        "<button onclick=\"sendRequest('/flash2')\">FLASH2</button></body></html>";
        request->send(200, "text/html", html);
    });

    // /on URLにアクセスしたときの処理
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        isFlashing = false; // 点滅を停止
        digitalWrite(LED_PIN, HIGH); // LEDを点灯
        request->send(200, "text/plain", "LED is ON");
    });

    // /off URLにアクセスしたときの処理
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        isFlashing = false; // 点滅を停止
        digitalWrite(LED_PIN, LOW); // LEDを消灯
        request->send(200, "text/plain", "LED is OFF");
    });

    // /flash URLにアクセスしたときの処理
    server.on("/flash", HTTP_GET, [](AsyncWebServerRequest *request) {
        isFlashing = true; // 点滅を開始
        flashInterval = 1000; // 点滅間隔を1秒に設定
        request->send(200, "text/plain", "LED is FLASHING");
    });

    // /flash2 URLにアクセスしたときの処理
    server.on("/flash2", HTTP_GET, [](AsyncWebServerRequest *request) {
        isFlashing = true; // 点滅を開始
        flashInterval = 2000; // 点滅間隔を2秒に設定
        request->send(200, "text/plain", "LED is FLASHING with 2 seconds interval");
    });

    // サーバーを開始
    server.begin();
}

void loop() {
    // LEDの点滅処理を呼び出す
    ledFlash();
}