#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ---- Settings ----
const char* AP_SSID = "ScreenCam";
const char* AP_PASS = "12345678";
IPAddress camIP(192, 168, 4, 50);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// ---- Picture orientation and effects ----
const bool FLIP_VERTICAL   = true;   // true = turn the picture upside down
const bool FLIP_HORIZONTAL = false;  // true = mirror left-right
const bool NEGATIVE_COLORS = false;  // true = invert the colors (like a photo negative)

// ---- AI Thinker ESP32-CAM pins ----
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_PIN          4

WebServer server(80);
bool flashState = false;
unsigned long lastWifiCheck = 0;

bool startCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;   // 640x480. Must match FRAME_W/FRAME_H on the screen.
  config.jpeg_quality = 12;            // lower number = better quality, bigger file
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, FLIP_VERTICAL ? 1 : 0);
  s->set_hmirror(s, FLIP_HORIZONTAL ? 1 : 0);
  s->set_special_effect(s, NEGATIVE_COLORS ? 1 : 0);   // 0 = normal, 1 = negative
  return true;
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(503, "text/plain", "capture failed");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleFlash() {
  if (server.hasArg("state")) {
    flashState = (server.arg("state") == "1");
    digitalWrite(FLASH_PIN, flashState ? HIGH : LOW);
  }
  server.send(200, "text/plain", flashState ? "on" : "off");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.config(camIP, gateway, subnet, gateway);
  WiFi.begin(AP_SSID, AP_PASS);
  Serial.print("Connecting to ScreenCam");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, camera IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Not connected yet, will keep trying");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  if (!startCamera()) {
    Serial.println("Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  connectWifi();

  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/flash", HTTP_GET, handleFlash);
  server.begin();
  Serial.println("Camera server started");
}

void loop() {
  server.handleClient();

  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      WiFi.reconnect();
    }
  }
}
