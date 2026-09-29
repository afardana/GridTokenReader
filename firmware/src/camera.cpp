#include "camera.h"

#include <Arduino.h>

#include "board_pins.h"
#include "config.h"

bool cameraInit() {
  pinMode(PIN_FLASH_LED, OUTPUT);
  flashSet(false);

  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_pwdn = PIN_CAM_PWDN;
  c.pin_reset = PIN_CAM_RESET;
  c.pin_xclk = PIN_CAM_XCLK;
  c.pin_sccb_sda = PIN_CAM_SIOD;
  c.pin_sccb_scl = PIN_CAM_SIOC;
  c.pin_d7 = PIN_CAM_Y9;
  c.pin_d6 = PIN_CAM_Y8;
  c.pin_d5 = PIN_CAM_Y7;
  c.pin_d4 = PIN_CAM_Y6;
  c.pin_d3 = PIN_CAM_Y5;
  c.pin_d2 = PIN_CAM_Y4;
  c.pin_d1 = PIN_CAM_Y3;
  c.pin_d0 = PIN_CAM_Y2;
  c.pin_vsync = PIN_CAM_VSYNC;
  c.pin_href = PIN_CAM_HREF;
  c.pin_pclk = PIN_CAM_PCLK;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = CAPTURE_FRAMESIZE;
  c.jpeg_quality = CAPTURE_JPEG_QUALITY;

  if (psramFound()) {
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.fb_count = 2;
    c.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("[cam] no PSRAM - falling back to VGA in DRAM");
    c.frame_size = FRAMESIZE_VGA;
    c.fb_location = CAMERA_FB_IN_DRAM;
    c.fb_count = 1;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[cam] init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  Serial.printf("[cam] sensor PID 0x%02x, PSRAM %s\n", s->id.PID, psramFound() ? "yes" : "no");
  return true;
}

camera_fb_t *cameraCapture(bool useFlash) {
  if (useFlash) {
    flashSet(true);
    delay(FLASH_SETTLE_MS);
  }
  // The first frame may have been exposed before the flash came on — drop it.
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  if (useFlash) flashSet(false);
  return fb;
}

void flashSet(bool on) { digitalWrite(PIN_FLASH_LED, on ? HIGH : LOW); }
