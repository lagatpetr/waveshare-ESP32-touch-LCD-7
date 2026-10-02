#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <Waveshare_ST7262_LVGL.h>
#include <lvgl.h>
#include "esp_heap_caps.h"

// ---- Settings ----
const char* AP_SSID = "ScreenCam";
const char* AP_PASS = "12345678";
const char* CAM_IP  = "192.168.4.50";

#define FRAME_W 640            // must match the camera frame size (VGA = 640x480)
#define FRAME_H 480
#define MAX_JPEG_SIZE (120 * 1024)
const bool SWAP_BYTES = false; // set to true if the picture colors look wrong

// ---- Globals ----
uint16_t* bufA = nullptr;
uint16_t* bufB = nullptr;
uint16_t* front = nullptr;      // buffer currently shown on screen
uint16_t* decodeBuf = nullptr;  // buffer being decoded into
uint8_t*  jpegBuf = nullptr;

lv_obj_t* canvas;
lv_obj_t* statusLabel;
lv_obj_t* flashBtn;
lv_obj_t* flashLabel;

volatile int flashWanted = -1;  // -1 = nothing to send, 0 = off, 1 = on
bool flashOn = false;
bool camUp = false;

unsigned long fpsStart = 0;
int frames = 0;

// TJpg_Decoder calls this for each block of decoded pixels
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!decodeBuf) return false;
  for (int row = 0; row < h; row++) {
    int dy = y + row;
    if (dy >= FRAME_H) break;
    int copyW = w;
    if (x + copyW > FRAME_W) copyW = FRAME_W - x;
    if (copyW <= 0) continue;
    memcpy(&decodeBuf[dy * FRAME_W + x], &bitmap[row * w], copyW * 2);
  }
  return true;
}

void setStatus(const char* text) {
  lvgl_port_lock(-1);
  lv_label_set_text(statusLabel, text);
  lvgl_port_unlock();
}

void onFlashClicked(lv_event_t* e) {
  flashOn = !flashOn;
  flashWanted = flashOn ? 1 : 0;
  lv_obj_set_style_bg_color(flashBtn,
    lv_color_hex(flashOn ? 0x00B050 : 0x606060), 0);
  lv_label_set_text(flashLabel, flashOn ? "FLASH: ON" : "FLASH: OFF");
}

bool sendFlash(int state) {
  HTTPClient http;
  http.setTimeout(2000);
  if (!http.begin(String("http://") + CAM_IP + "/flash?state=" + state)) return false;
  int code = http.GET();
  http.end();
  return code == HTTP_CODE_OK;
}

bool fetchFrame() {
  HTTPClient http;
  http.setTimeout(3000);
  http.setReuse(false);
  if (!http.begin(String("http://") + CAM_IP + "/capture")) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  int len = http.getSize();
  if (len <= 0 || len > MAX_JPEG_SIZE) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int got = stream->readBytes(jpegBuf, len);
  http.end();
  if (got != len) return false;

  uint16_t* target = (front == bufA) ? bufB : bufA;
  decodeBuf = target;
  if (TJpgDec.drawJpg(0, 0, jpegBuf, len) != 0) return false;

  lvgl_port_lock(-1);
  lv_canvas_set_buffer(canvas, target, FRAME_W, FRAME_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_invalidate(canvas);
  lvgl_port_unlock();
  front = target;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Screen + ESP32-CAM viewer");
  Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());

  bufA = (uint16_t*)heap_caps_malloc(FRAME_W * FRAME_H * 2, MALLOC_CAP_SPIRAM);
  bufB = (uint16_t*)heap_caps_malloc(FRAME_W * FRAME_H * 2, MALLOC_CAP_SPIRAM);
  jpegBuf = (uint8_t*)heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM);
  if (!bufA || !bufB || !jpegBuf) {
    Serial.println("Memory allocation failed - check that PSRAM is set to OPI PSRAM");
    while (true) delay(1000);
  }
  memset(bufA, 0, FRAME_W * FRAME_H * 2);
  memset(bufB, 0, FRAME_W * FRAME_H * 2);
  front = bufA;

  lcd_init();

  // ---- WiFi: the screen creates its own network ----
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.print("Access point started, IP: ");
  Serial.println(WiFi.softAPIP());

  // ---- JPEG decoder ----
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(SWAP_BYTES);
  TJpgDec.setCallback(tftOutput);

  // ---- User interface ----
  lvgl_port_lock(-1);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101010), 0);

  canvas = lv_canvas_create(lv_scr_act());
  lv_canvas_set_buffer(canvas, front, FRAME_W, FRAME_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(canvas, 0, 0);

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_obj_set_width(title, 150);
  lv_obj_set_pos(title, 648, 20);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(title, "ESP32-CAM\nWiFi: ScreenCam");

  statusLabel = lv_label_create(lv_scr_act());
  lv_obj_set_width(statusLabel, 150);
  lv_obj_set_pos(statusLabel, 648, 90);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFFD000), 0);
  lv_label_set_text(statusLabel, "Camera: waiting...");

  flashBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(flashBtn, 140, 120);
  lv_obj_set_pos(flashBtn, 655, 300);
  lv_obj_set_style_bg_color(flashBtn, lv_color_hex(0x606060), 0);
  lv_obj_set_style_radius(flashBtn, 10, 0);
  lv_obj_add_event_cb(flashBtn, onFlashClicked, LV_EVENT_CLICKED, NULL);

  flashLabel = lv_label_create(flashBtn);
  lv_label_set_text(flashLabel, "FLASH: OFF");
  lv_obj_center(flashLabel);

  lvgl_port_unlock();

  fpsStart = millis();
  Serial.println("Ready. Waiting for the camera to join.");
}

void loop() {
  // LVGL runs in the library's own task, so lv_timer_handler() is not called here.

  // Send a pending flash command from the button
  if (flashWanted != -1) {
    int wanted = flashWanted;
    if (sendFlash(wanted) && flashWanted == wanted) {
      flashWanted = -1;
    }
  }

  // Get and show one frame
  if (fetchFrame()) {
    frames++;
    if (!camUp) {
      camUp = true;
      flashWanted = flashOn ? 1 : 0;   // re-sync the flash after a reconnect
      Serial.println("Camera connected");
    }
  } else {
    if (camUp || frames == 0) {
      camUp = false;
      setStatus("Camera: waiting...");
    }
    delay(500);
  }

  // Show frames per second every 2 seconds
  unsigned long now = millis();
  if (camUp && now - fpsStart >= 2000) {
    char text[40];
    snprintf(text, sizeof(text), "Camera: OK\n%.1f fps", frames * 1000.0f / (now - fpsStart));
    setStatus(text);
    frames = 0;
    fpsStart = now;
  }
}
