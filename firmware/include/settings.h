#pragma once

#include <Arduino.h>

// Runtime-tunable capture settings, persisted in NVS. Defaults come from config.h.
// The meter sits in a sealed, dark compartment. Its own LCD backlight gives the
// cleanest image (no glare); the flash LED is the fallback when the backlight is
// off. Light source for captures:
//   AUTO:      shoot by the meter's own LCD backlight; if the LCD region is darker
//              than backlightMinLuma (backlight off, e.g. after a blackout until a
//              key is pressed), reshoot with the flash LED.
//   BACKLIGHT: never use the LED.   LED: always use the LED.
enum LightMode : uint8_t { LIGHT_AUTO = 0, LIGHT_BACKLIGHT = 1, LIGHT_LED = 2 };

struct Settings {
  uint8_t lightMode;     // LightMode
  uint8_t backlightMinLuma;  // AUTO threshold, mean luma 0-255 of the LCD region
  uint8_t ledDuty;       // flash LED PWM 0-255 (0 = off)
  uint16_t settleMs;     // LED-on time before the frame is taken (lets AEC converge)
  bool manualExposure;   // true = fixed exposure/gain (consistent under our own LED)
  uint16_t exposure;     // 0-1200, used when manualExposure
  uint8_t gain;          // 0-30, used when manualExposure
};

extern Settings settings;

void settingsLoad();
void settingsSave();
String settingsJson();
const char *lightModeName(uint8_t mode);
