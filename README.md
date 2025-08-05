# ESP32-S3 Motion-Activated Camera

This project uses an ESP32-S3 board (e.g. LilyGO T-Camera S3) to capture images when motion is detected and save them on a micro-SD card. After saving each image, the sketch waits 10 seconds before looking for motion again. The images are named using a timestamp, and the sketch sends the timestamp to a Wi-Fi endpoint.

## Features

- Uses the ESP32-S3 with built-in OV2640 camera and AS312 PIR sensor (e.g. LilyGO T-Camera S3).
- Captures a JPEG image when motion is detected.
- Saves images to a micro-SD card with filenames like `20250101_120301.jpg`.
- Reads Wi-Fi credentials from a `credentials.txt` file on the micro-SD card.
- Sends a GET request containing the timestamp to a configurable endpoint.
- Waits 10 seconds between detections to avoid capturing too many images.

## Hardware and preparation

- ESP32-S3 board with camera and PIR sensor (e.g. LilyGO T-Camera S3).
- Micro-SD card formatted as FAT-32.
- USB cable for programming.

Prepare the micro-SD card with a `credentials.txt` file in the root directory:

```
MyWiFiSSID
MyWiFiPassword
```

Each line must contain only the SSID and password (no extra spaces).

## Sketch

The Arduino sketch is provided in `main.ino`. It uses the `esp_camera` library, initializes the camera and SD card, configures a PIR input, reads the credentials file, sets up an NTP time source, and loops waiting for motion. When motion is detected, it captures an image, saves it to the SD card, sends a GET request with the timestamp, and delays 10 seconds.

Before compiling, edit the sketch to set:

- `PIR_PIN` if your PIR sensor is connected to a different GPIO.
- The endpoint URL inside the `loop()` function (`http://your-endpoint.example/api?...`) to your real server URL.
- Optionally adjust the frame size or JPEG quality.

## Building and uploading

1. Install the **ESP32** board package in the Arduino IDE.
2. Select the **ESP32S3 Dev Module** (or the specific board) in **Tools > Board**.
3. Install the required libraries: `esp_camera`, `WiFi`, `SD_MMC`, and `HTTPClient` (these come with the ESP32 core).
4. Open `main.ino`, update the configuration values, and upload it to your board.
5. Insert the prepared micro-SD card and power the board. Check the serial monitor for status messages.

## Notes

- The board must have PSRAM to buffer images efficiently (the LilyGO T-Camera S3 does).
- If you use a board without a PIR sensor, connect an external one to a free GPIO and update `PIR_PIN`.
- Timestamps are generated using NTP; ensure the board has internet access.
