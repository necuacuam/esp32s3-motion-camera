#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>

#include "time.h"

#define PIR_PIN          13      // adjust if your PIR is on a different GPIO
#define PIR_DETECTED     HIGH
#define CAMERA_MODEL_LILYGO_TCAMERA_S3
#include "camera_pins.h"         // defines pin numbers for this board

// NTP settings for Europe/Madrid (CET/CEST)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;        // UTC+1
const int   daylightOffset_sec = 3600;   // additional +1 h during daylight saving

String wifiSsid, wifiPass;

void readCredentials(String &ssid, String &password) {
  File file = SD_MMC.open("/credentials.txt");
  if (!file) {
    Serial.println("Failed to open credentials file");
    return;
  }
  ssid     = file.readStringUntil('\n');
  ssid.trim();
  password = file.readStringUntil('\n');
  password.trim();
  file.close();
}

void setup() {
  Serial.begin(115200);

  // Start SD card
  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  // Read Wi‑Fi credentials from SD card
  readCredentials(wifiSsid, wifiPass);

  // Connect to Wi‑Fi
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  Serial.println("Connecting to Wi‑Fi…");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWi‑Fi connected" : "\nWi‑Fi failed");

  // Configure time for timestamp filenames
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Configure PIR pin
  pinMode(PIR_PIN, INPUT);

  // Configure camera (pins defined in camera_pins.h)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
  }
}

void loop() {
  static unsigned long lastShot = 0;

  // wait for motion
  if (digitalRead(PIR_PIN) == PIR_DETECTED && (millis() - lastShot > 10000)) {
    lastShot = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // obtain current time for filename
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to get time, using millis");
    }
    char ts[32];
    strftime(ts, sizeof(ts), "/%Y%m%d_%H%M%S.jpg", &timeinfo);

    // Save to SD card
    fs::FS &fs = SD_MMC;
    File file = fs.open(ts, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file in write mode");
    } else {
      file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("Saved file: %s\n", ts);
    }

    esp_camera_fb_return(fb);

 

    // wait 10 seconds before next possible capture
    delay(10000);
  }
}
