#pragma once

#include "esp_camera.h"

bool cameraInit();

// Captures a fresh JPEG frame. Caller must release it with esp_camera_fb_return().
camera_fb_t *cameraCapture(bool useFlash);

void flashSet(bool on);
