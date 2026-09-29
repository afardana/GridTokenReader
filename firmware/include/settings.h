#pragma once

#include <Arduino.h>

// Runtime-tunable capture settings, persisted in NVS. Defaults come from config.h.
// The meter usually sits in a sealed, dark compartment, so the flash LED is the
// only light source: its intensity and the exposure strategy matter most.
struct Settings {
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
