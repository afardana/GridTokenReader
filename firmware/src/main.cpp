// GridTokenReader — ESP32-CAM that photographs a prepaid electricity meter's
// LCD and pushes the frame to a backend (Node-RED via MQTT and/or HTTPS).
// Phase 0 skeleton: capture + transport only; digit recognition comes later
// (see docs/ROADMAP.md).

#include <Arduino.h>

#include "camera.h"
#include "config.h"
#include "ingest.h"
#include "net.h"
#include "web.h"

static volatile bool pushRequested = true;  // push once as soon as we're online
static uint32_t lastPush = 0;

static void requestPush() { pushRequested = true; }

static void captureAndPush() {
  camera_fb_t *fb = cameraCapture(true);
  if (!fb) {
    Serial.println("[cam] capture failed");
    return;
  }
  bool viaMqtt = mqttPublishImage(fb->buf, fb->len);
  bool viaHttp = ingestPostImage(fb->buf, fb->len);
  Serial.printf("[push] %ux%u %u bytes - mqtt:%s http:%s\n", (unsigned)fb->width, (unsigned)fb->height,
                (unsigned)fb->len, mqttEnabled() ? (viaMqtt ? "ok" : "fail") : "off",
                ingestEnabled() ? (viaHttp ? "ok" : "fail") : "off");
  esp_camera_fb_return(fb);
  mqttPublishStatus();
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nGridTokenReader %s\n", FW_VERSION);

  if (!cameraInit()) {
    Serial.println("[cam] restarting in 10 s");
    delay(10000);
    ESP.restart();
  }
  netBegin(requestPush);
  webBegin(requestPush);
}

void loop() {
  netLoop();
  webLoop();

  bool anyTransport = mqttEnabled() || ingestEnabled();
  bool due = pushRequested || millis() - lastPush >= CAPTURE_INTERVAL_S * 1000UL;
  if (netWifiUp() && anyTransport && due) {
    pushRequested = false;
    lastPush = millis();
    captureAndPush();
  }
  delay(2);
}
