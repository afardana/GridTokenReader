// GridTokenReader — ESP32-CAM that photographs a prepaid electricity meter's
// LCD and pushes the frame to a backend (Node-RED via MQTT and/or HTTPS).
// Phase 0 skeleton: capture + transport only; digit recognition comes later
// (see docs/ROADMAP.md). Firmware updates over the air: see ota.h.

#include <Arduino.h>

#include "camera.h"
#include "config.h"
#include "ingest.h"
#include "net.h"
#include "ota.h"
#include "settings.h"
#include "storage.h"
#include "web.h"

static volatile bool pushRequested = true;  // push once as soon as we're online
static uint32_t lastPush = 0;

static void requestPush() { pushRequested = true; }

static void captureAndPush() {
  CaptureInfo info;
  camera_fb_t *fb = cameraCaptureAuto(info);
  if (!fb) {
    Serial.println("[cam] capture failed");
    return;
  }
  char captureJson[80];
  snprintf(captureJson, sizeof captureJson, "{\"light\":\"%s\",\"lcd_luma\":%d}",
           info.usedLed ? "led" : "backlight", info.lcdLuma);
  bool onSd = storageSaveJpeg(fb->buf, fb->len);
  bool viaMqtt = mqttPublishImage(fb->buf, fb->len);
  bool viaHttp = ingestPostImage(fb->buf, fb->len, captureJson);
  Serial.printf("[push] %ux%u %u bytes %s - sd:%s mqtt:%s http:%s\n", (unsigned)fb->width, (unsigned)fb->height,
                (unsigned)fb->len, captureJson, storageReady() ? (onSd ? "ok" : "fail") : "off",
                mqttEnabled() ? (viaMqtt ? "ok" : "fail") : "off", ingestEnabled() ? (viaHttp ? "ok" : "fail") : "off");
  esp_camera_fb_return(fb);
  mqttPublishStatus();
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nGridTokenReader %s\n", FW_VERSION);

  settingsLoad();
  Serial.printf("[cfg] %s\n", settingsJson().c_str());
  // Wi-Fi first: its RF calibration burst plus a streaming camera is the classic
  // ESP32-CAM brownout on marginal 5 V supplies. Stagger the two current peaks.
  netBegin(requestPush);
  delay(WIFI_CAMERA_STAGGER_MS);
  storageBegin();  // before the camera: the LED's LEDC attach on GPIO 4 must come last
  if (!cameraInit()) {
    Serial.println("[cam] restarting in 10 s");
    delay(10000);
    ESP.restart();
  }
  webBegin(requestPush);
}

void loop() {
  netLoop();
  webLoop();
  if (netWifiUp()) otaBegin();
  otaLoop();
  if (otaInProgress()) return;  // keep flash/CPU/Wi-Fi for the update

  bool anyTransport = mqttEnabled() || ingestEnabled() || storageReady();
  bool due = pushRequested || millis() - lastPush >= CAPTURE_INTERVAL_S * 1000UL;
  // Captures need Wi-Fi only for NTP-dated SD names and pushes; SD-only works offline.
  if ((netWifiUp() || storageReady()) && anyTransport && due) {
    pushRequested = false;
    lastPush = millis();
    captureAndPush();
  }
  delay(2);
}
