#include "camera.h"

#include <Arduino.h>
#include <jpeg_decoder.h>

#include "board_pins.h"
#include "config.h"
#include "settings.h"

bool cameraInit() {
  // PWM on an explicit channel/timer so it can't collide with the camera XCLK.
  ledcAttachChannel(PIN_FLASH_LED, FLASH_LEDC_FREQ_HZ, 8, FLASH_LEDC_CHANNEL);
  flashSet(0);

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
  cameraApplySettings();
  return true;
}

void cameraApplySettings() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  if (settings.manualExposure) {
    // Under our own LED in a sealed box the scene never changes: fixed exposure
    // gives identical frames every time, which is what the recogniser wants.
    s->set_exposure_ctrl(s, 0);
    s->set_aec_value(s, settings.exposure);
    s->set_gain_ctrl(s, 0);
    s->set_agc_gain(s, settings.gain);
  } else {
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
  }
}

static bool ledWasOn = false;

camera_fb_t *cameraCapture(uint8_t ledDuty) {
  if (ledDuty > 0) flashSet(ledDuty);
  // Let AEC/AWB converge whenever the lighting changes (LED on, or first shot after it).
  if (ledDuty > 0 || ledWasOn) delay(settings.settleMs);
  ledWasOn = ledDuty > 0;
  // The oldest buffered frame may predate the LED or the settled exposure — drop it.
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  if (ledDuty > 0) flashSet(0);
  return fb;
}

void flashSet(uint8_t duty) { ledcWrite(PIN_FLASH_LED, duty); }

int cameraRegionLuma(const camera_fb_t *fb) {
  if (!fb || fb->format != PIXFORMAT_JPEG) return -1;
  const uint16_t w = (fb->width + 7) / 8, h = (fb->height + 7) / 8;
  const size_t outSize = (size_t)w * h * 3;
  uint8_t *rgb = (uint8_t *)ps_malloc(outSize);
  if (!rgb) return -1;
  esp_jpeg_image_cfg_t cfg = {};
  cfg.indata = fb->buf;
  cfg.indata_size = fb->len;
  cfg.outbuf = rgb;
  cfg.outbuf_size = outSize;
  cfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
  cfg.out_scale = JPEG_IMAGE_SCALE_1_8;
  esp_jpeg_image_output_t out = {};
  int luma = -1;
  if (esp_jpeg_decode(&cfg, &out) == ESP_OK && out.width && out.height) {
    // Central 60% x 50%: where the LCD sits in a correctly aimed frame.
    const int x0 = out.width / 5, x1 = out.width * 4 / 5, y0 = out.height / 4, y1 = out.height * 3 / 4;
    uint32_t sum = 0, n = 0;
    for (int y = y0; y < y1; y++) {
      const uint8_t *p = rgb + ((size_t)y * out.width + x0) * 3;
      for (int x = x0; x < x1; x++, p += 3, n++) sum += (p[0] + 2 * p[1] + p[2]) / 4;  // G in the middle either way
    }
    luma = n ? sum / n : -1;
  }
  free(rgb);
  return luma;
}

camera_fb_t *cameraCaptureAuto(CaptureInfo &info) {
  info = CaptureInfo();
  if (settings.lightMode == LIGHT_LED) {
    info.usedLed = settings.ledDuty > 0;
    return cameraCapture(settings.ledDuty);
  }
  camera_fb_t *fb = cameraCapture(0);
  info.lcdLuma = cameraRegionLuma(fb);
  if (settings.lightMode == LIGHT_BACKLIGHT || !fb || settings.ledDuty == 0 ||
      info.lcdLuma < 0 || info.lcdLuma >= settings.backlightMinLuma) {
    return fb;
  }
  // Backlight is off (e.g. after a blackout): light the LCD ourselves.
  esp_camera_fb_return(fb);
  info.usedLed = true;
  return cameraCapture(settings.ledDuty);
}
