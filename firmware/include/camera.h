#pragma once

#include "esp_camera.h"

bool cameraInit();

// Captures a fresh JPEG frame lit by the flash LED at `ledDuty` (0 = no LED).
// Caller must release it with esp_camera_fb_return().
camera_fb_t *cameraCapture(uint8_t ledDuty);

// Applies exposure settings (auto vs. manual) from `settings` to the sensor.
void cameraApplySettings();

void flashSet(uint8_t duty);
