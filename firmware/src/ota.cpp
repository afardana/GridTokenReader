#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "camera.h"
#include "config.h"
#include "net.h"

static bool started = false;
static volatile bool inProgress = false;

bool otaEnabled() { return strlen(OTA_PASSWORD) > 0; }
bool otaInProgress() { return inProgress; }

void otaSetInProgress(bool active) {
  inProgress = active;
  if (active) flashSet(0);
}

bool otaLooksLikeAppImage(const uint8_t *buf, size_t len) {
  // 24-byte image header + 8-byte segment header, then esp_app_desc_t whose
  // magic word is 0xABCD5432 (little endian). The bootloader lacks it.
  return len >= 36 && buf[0] == 0xE9 && buf[32] == 0x32 && buf[33] == 0x54 && buf[34] == 0xCD && buf[35] == 0xAB;
}

void otaBegin() {
  if (started) return;
  if (!otaEnabled()) {
    Serial.println("[ota] disabled (OTA_PASSWORD not set)");
    started = true;
    return;
  }

  ArduinoOTA.setHostname(netDeviceId().c_str());
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setMdnsEnabled(false);  // net.cpp owns mDNS and advertises _arduino._tcp

  ArduinoOTA.onStart([] {
    otaSetInProgress(true);
    Serial.println("[ota] update started");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static unsigned lastPct = 101;
    unsigned pct = total ? done * 100 / total : 0;
    if (pct % 10 == 0 && pct != lastPct) Serial.printf("[ota] %u%%\n", pct);
    lastPct = pct;
  });
  ArduinoOTA.onEnd([] { Serial.println("[ota] done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[ota] error %u\n", (unsigned)err);
    otaSetInProgress(false);
  });

  ArduinoOTA.begin();
  started = true;
  Serial.printf("[ota] ready: %s.local:3232 and http://%s.local/update\n", netDeviceId().c_str(),
                netDeviceId().c_str());
}

void otaLoop() {
  if (started && otaEnabled()) ArduinoOTA.handle();
}
