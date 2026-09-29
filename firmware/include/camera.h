#pragma once

#include "esp_camera.h"

bool cameraInit();

// Captures a fresh JPEG frame lit by the flash LED at `ledDuty` (0 = no LED).
// Caller must release it with esp_camera_fb_return().
camera_fb_t *cameraCapture(uint8_t ledDuty);

struct CaptureInfo {
  bool usedLed = false;
  int lcdLuma = -1;  // mean luma of the LCD region of the no-LED shot (-1 = not measured)
};

// Captures according to settings.lightMode (see settings.h) and reports what it did.
camera_fb_t *cameraCaptureAuto(CaptureInfo &info);

// Mean luma (0-255) of the central LCD region, from a 1/8-scale decode. -1 on error.
int cameraRegionLuma(const camera_fb_t *fb);

// Applies exposure settings (auto vs. manual) from `settings` to the sensor.
void cameraApplySettings();

void flashSet(uint8_t duty);
