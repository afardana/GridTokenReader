#pragma once

#define FW_VERSION "0.1.0"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found - building with secrets.example.h placeholders"
#include "secrets.example.h"
#endif

// How often a frame is captured and pushed (seconds).
#ifndef CAPTURE_INTERVAL_S
#define CAPTURE_INTERVAL_S 300
#endif

// SVGA (800x600) is plenty for a meter LCD at ~10 cm; raise if digits are tiny.
#define CAPTURE_FRAMESIZE    FRAMESIZE_SVGA
#define CAPTURE_JPEG_QUALITY 12      // 0-63, lower = better quality / bigger file
#define FLASH_SETTLE_MS      150     // let AEC adapt after the flash LED turns on

#define MQTT_TOPIC_BASE "gridtoken"  // topics: gridtoken/<device-id>/...
