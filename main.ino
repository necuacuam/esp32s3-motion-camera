#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include "time.h"
#include "esp_http_server.h"  // HTTP server for streaming

#define RADAR_PIN          13      // adjust if your radar sensor's OUT pin is on a different GPIO
#define RADAR_DETECTED     HIGH
#define CAMERA_MODEL_LILYGO_TCAMERA_S3
#include "camera_pins.h"         // defines pin numbers for this board

// NTP settings for Europe/Madrid (CET/CEST)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;        // UTC+1
const int   daylightOffset_sec = 3600;   // additional +1 h during daylight saving

String wifiSsid, wifiPass;

// HTTP server handle for camera stream
static httpd_handle_t stream_httpd = NULL;
// Start time of the initial streaming period (0 if not streaming)
static unsigned long stream_start_time = 0;

// Boundaries and headers for multipart MJPEG stream
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// HTTP handler that streams JPEG frames continuously
static esp_err_t stream_handler(httpd_req_t *req) {
  // Set content type to multipart
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    // If the 10 minute streaming period has expired, break out of the stream loop
    if (stream_start_time != 0 && (millis() - stream_start_time) >= 600000UL) {
      break;
    }
    // Capture a frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      // If capture failed, send an empty chunk to allow client to recover
      httpd_resp_send_chunk(req, "", 0);
      continue;
    }
    // Send boundary
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }
    // Prepare and send the JPEG header with length
    char header_buf[64];
    size_t header_len = snprintf(header_buf, sizeof(header_buf), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, header_buf, header_len);
    if (res != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }
    // Send JPEG buffer
    res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) {
      break;
    }
    // Send empty chunk to indicate end of part
    res = httpd_resp_send_chunk(req, "", 0);
    if (res != ESP_OK) {
      break;
    }
    // Yield to other tasks
    delay(10);
  }
  return ESP_OK;
}

// Start the camera HTTP stream server
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  // URI handler for the stream at root path
  httpd_uri_t stream_uri = {
    .uri       = "/",        // root serves the MJPEG stream directly
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  // Start the server
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

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

  // Configure radar pin
  pinMode(RADAR_PIN, INPUT);

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
  config.xclk_freq_hz = 20000000;               // 20 MHz XCLK
  config.pixel_format = PIXFORMAT_JPEG;         // JPEG for saving to SD
  config.frame_size   = FRAMESIZE_VGA;          // adjust resolution as needed
  config.jpeg_quality = 12;                     // 0 (best) – 63 (worst)
  config.fb_count     = 2;                      // two frame buffers if PSRAM is present
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

  // Start the streaming server and record the start time for the 10 minute setup window
  startCameraServer();
  stream_start_time = millis();
  Serial.println("Camera stream available at http://<device_ip>/");
}

void loop() {
  // During the first 10 minutes after boot, provide a live camera stream for alignment
  if (stream_start_time != 0) {
    // If still within 10 minutes, do nothing else
    if ((millis() - stream_start_time) < 600000UL) {
      delay(100);
      return;
    } else {
      // Stop the HTTP server after 10 minutes and clear the flag
      if (stream_httpd) {
        httpd_stop(stream_httpd);
        stream_httpd = NULL;
      }
      stream_start_time = 0;
      Serial.println("Streaming period ended. Entering motion capture mode.");
    }
  }

  static unsigned long lastShot = 0;

  // Wait for motion
  if (digitalRead(RADAR_PIN) == RADAR_DETECTED && (millis() - lastShot > 10000)) {
    lastShot = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Obtain current time for filename
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to get time, using millis");
    }
    char ts[32];
    // Format: YYYYMMDD_HHMMSS.jpg
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

    // Wait 10 seconds before next possible capture
    delay(10000);
  }
}
