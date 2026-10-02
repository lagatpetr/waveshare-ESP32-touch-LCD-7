# Camera - Waveshare ESP32-S3-Touch-LCD-7

An ESP32-CAM video viewer for the Waveshare ESP32-S3-Touch-LCD-7 touch screen.

- The touch screen creates its own WiFi network (`ScreenCam`).
- The ESP32-CAM joins that network with a fixed IP (`192.168.4.50`).
- The screen fetches pictures from the camera and displays them (640x480, a few frames per second).
- A touch button on the screen switches the ESP32-CAM flash light on and off.

## Folders

| Folder | Board | What it does |
|---|---|---|
| `screen_viewer/` | Waveshare ESP32-S3-Touch-LCD-7 | WiFi access point, picture display, flash button |
| `esp32_cam_server/` | AI Thinker ESP32-CAM | Joins the WiFi, serves pictures and the flash switch |

## Libraries (Arduino IDE)

Screen board:

- `Waveshare_ST7262_LVGL`
- `ESP32_Display_Panel` (0.1.4 or higher)
- `ESP32_IO_Expander` (0.0.3)
- `lvgl` (8.3.11), with an `lv_conf.h` file in the Arduino `libraries` folder next to the `lvgl` folder
- `TJpg_Decoder` by Bodmer

ESP32-CAM: no extra libraries.

Use the esp32 by Espressif board package, version 3.0.6 or higher.

## Board settings

Screen board (`screen_viewer`):

- PSRAM: OPI PSRAM
- Flash Size: 16MB with a 16MB partition scheme
- USB CDC On Boot: Disabled (when uploading through the UART port)

ESP32-CAM (`esp32_cam_server`):

- Board: ESP32 Wrover Module
- PSRAM: Enabled
- Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)

## How to run

1. Upload `esp32_cam_server` to the ESP32-CAM and power it from a good 5V 2A supply.
2. Upload `screen_viewer` to the screen board and press RESET once.
3. The camera joins the screen's WiFi within about 20 seconds. The right side of the screen shows "Camera: OK" and the picture appears on the left.

## Settings you can change

In `esp32_cam_server.ino`:

- `FLIP_VERTICAL`, `FLIP_HORIZONTAL`: turn the picture upside down or mirror it
- `NEGATIVE_COLORS`: invert the colors

In `screen_viewer.ino`:

- `SWAP_BYTES`: set to `true` if the picture colors look wrong

To use a smaller picture, set `FRAMESIZE_QVGA` in the camera code, and `FRAME_W 320` and `FRAME_H 240` in the screen code. Both must always match.

## WiFi password

The default WiFi name and password (`ScreenCam` / `12345678`) are written in both sketches. Change `AP_SSID` and `AP_PASS` in both files, and keep them identical, before using this anywhere other than a desk test.
