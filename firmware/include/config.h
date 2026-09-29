#pragma once

#define FW_VERSION "0.3.0"

// Credentials and endpoints come from the repo-root `.env` (see .env.example),
// turned into env_secrets.h by scripts/load_env.py at build time.
#if __has_include("env_secrets.h")
#include "env_secrets.h"
#endif

#ifndef WIFI_SSID
#warning "WIFI_SSID not set - create .env from .env.example"
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// --- Transport A: MQTT (disabled when MQTT_HOST is empty) ---------------------
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

// --- Transport B: HTTP(S) POST of each JPEG (disabled when INGEST_URL is empty)
#ifndef INGEST_URL
#define INGEST_URL ""
#endif
// Connect to this IP instead of resolving the URL host, while still using the
// URL host for TLS SNI + certificate validation. Keeps traffic on the LAN when
// DNS returns a public address (e.g. reverse proxy on the Node-RED box).
#ifndef INGEST_CONNECT_IP
#define INGEST_CONNECT_IP ""
#endif
// Preferred auth: per-device bearer token checked by the reverse proxy
// (see docs/NGINX.md). HTTP basic auth (INGEST_USER/PASSWORD) is the fallback.
#ifndef INGEST_TOKEN
#define INGEST_TOKEN ""
#endif
#ifndef INGEST_USER
#define INGEST_USER ""
#endif
#ifndef INGEST_PASSWORD
#define INGEST_PASSWORD ""
#endif
// HTTPS trusts ISRG Root X1 (Let's Encrypt) unless INGEST_CA_CERT is defined.

// --- OTA (ArduinoOTA on :3232 + web upload at /update; disabled when empty) --
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif
#define OTA_WEB_USER "admin"

// How often a frame is captured and pushed (seconds).
#ifndef CAPTURE_INTERVAL_S
#define CAPTURE_INTERVAL_S 300
#endif

// SVGA (800x600) is plenty for a meter LCD at ~10 cm; raise if digits are tiny.
#define CAPTURE_FRAMESIZE    FRAMESIZE_SVGA
#define CAPTURE_JPEG_QUALITY 12      // 0-63, lower = better quality / bigger file

// Flash LED defaults (tunable at runtime via /settings, persisted in NVS).
// The meter LCD sits in a dark, sealed compartment: the LED is the only light.
// Full power at ~10 cm tends to blow out / glare on the LCD, so start mid-way.
#define FLASH_LED_DUTY       128     // 0-255 PWM
#define FLASH_SETTLE_MS      600     // auto-exposure needs several frames to converge from dark
#define FLASH_LEDC_CHANNEL   7       // camera XCLK owns LEDC channel 0 / timer 0
#define FLASH_LEDC_FREQ_HZ   40000   // well above line rate: 5 kHz caused rolling-shutter banding

// microSD rolling buffer (1-bit mode, see storage.h). Oldest captures are
// deleted first so at least SD_MIN_FREE_PCT of the card always stays free.
#define SD_CAPTURE_DIR          "/captures"
#ifndef SD_MIN_FREE_PCT
#define SD_MIN_FREE_PCT         10
#endif
#define SD_MAX_DELETES_PER_SAVE 50

// Power: ESP32-CAM + flash LED + Wi-Fi TX peaks easily brown out weak 5 V supplies.
#ifndef WIFI_TX_POWER
#define WIFI_TX_POWER WIFI_POWER_15dBm   // default is 19.5 dBm
#endif
#define WIFI_CAMERA_STAGGER_MS 1500      // let Wi-Fi calibrate before the camera starts streaming

#define MQTT_TOPIC_BASE "gridtoken"  // topics: gridtoken/<device-id>/...
