#pragma once

#include <Arduino.h>

// HTTP(S) POST of a JPEG to INGEST_URL (e.g. a Node-RED "http in" node).
// Headers: Authorization (Bearer INGEST_TOKEN or Basic), Content-Type: image/jpeg,
// X-Device-Id, X-Firmware, X-Device-Status (the /status JSON).
bool ingestEnabled();
bool ingestPostImage(const uint8_t *buf, size_t len);
