#pragma once

#include <stddef.h>
#include <stdint.h>

// Over-the-air updates, both password-protected with OTA_PASSWORD:
//   * ArduinoOTA on UDP/TCP 3232 - `pio run -e esp32cam-ota -t upload`
//   * browser upload at http://<device>/update (user OTA_WEB_USER), see web.cpp
bool otaEnabled();
void otaBegin();  // idempotent; call once Wi-Fi is up
void otaLoop();

// True while an update is being written; captures pause meanwhile.
bool otaInProgress();
void otaSetInProgress(bool active);

// True if `buf` starts like an ESP32 *application* image (not bootloader / factory.bin).
bool otaLooksLikeAppImage(const uint8_t *buf, size_t len);
