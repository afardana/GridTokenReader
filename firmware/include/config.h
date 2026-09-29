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

// Flash LED defaults (tunable at runtime via /settings, persisted in NVS).
// The meter LCD sits in a dark, sealed compartment: the LED is the only light.
// Full power at ~10 cm tends to blow out / glare on the LCD, so start mid-way.
#define FLASH_LED_DUTY       128     // 0-255 PWM
#define FLASH_SETTLE_MS      600     // auto-exposure needs several frames to converge from dark
#define FLASH_LEDC_CHANNEL   7       // camera XCLK owns LEDC channel 0 / timer 0
#define FLASH_LEDC_FREQ_HZ   5000

#define MQTT_TOPIC_BASE "gridtoken"  // topics: gridtoken/<device-id>/...
