#pragma once

#include <Arduino.h>

// HTTP(S) POST of a JPEG to INGEST_URL (e.g. a Node-RED "http in" node).
// Headers: Authorization (Bearer INGEST_TOKEN or Basic), Content-Type: image/jpeg,
// X-Device-Id, X-Firmware, X-Device-Status (the /status JSON), X-Capture.
bool ingestEnabled();
// `captureJson` goes into X-Capture (how the frame was lit, LCD brightness).
bool ingestPostImage(const uint8_t *buf, size_t len, const String &captureJson);
