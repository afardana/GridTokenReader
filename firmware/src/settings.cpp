#include "settings.h"

#include <Preferences.h>

#include "config.h"

Settings settings = {LIGHT_AUTO, BACKLIGHT_MIN_LUMA, FLASH_LED_DUTY, FLASH_SETTLE_MS, false, 300, 0};

static Preferences prefs;

void settingsLoad() {
  prefs.begin("gridtoken", false);  // RW so the namespace exists on first boot
  settings.lightMode = prefs.getUChar("light", settings.lightMode);
  settings.backlightMinLuma = prefs.getUChar("bl_luma", settings.backlightMinLuma);
  settings.ledDuty = prefs.getUChar("led", settings.ledDuty);
  settings.settleMs = prefs.getUShort("settle", settings.settleMs);
  settings.manualExposure = prefs.getBool("aec_man", settings.manualExposure);
  settings.exposure = prefs.getUShort("exposure", settings.exposure);
  settings.gain = prefs.getUChar("gain", settings.gain);
  prefs.end();
}

void settingsSave() {
  prefs.begin("gridtoken", false);
  prefs.putUChar("light", settings.lightMode);
  prefs.putUChar("bl_luma", settings.backlightMinLuma);
  prefs.putUChar("led", settings.ledDuty);
  prefs.putUShort("settle", settings.settleMs);
  prefs.putBool("aec_man", settings.manualExposure);
  prefs.putUShort("exposure", settings.exposure);
  prefs.putUChar("gain", settings.gain);
  prefs.end();
}

const char *lightModeName(uint8_t mode) {
  return mode == LIGHT_BACKLIGHT ? "backlight" : mode == LIGHT_LED ? "led" : "auto";
}

String settingsJson() {
  char json[224];
  snprintf(json, sizeof json,
           "{\"light\":\"%s\",\"backlight_min_luma\":%u,\"led\":%u,\"settle_ms\":%u,"
           "\"exposure_mode\":\"%s\",\"exposure\":%u,\"gain\":%u}",
           lightModeName(settings.lightMode), (unsigned)settings.backlightMinLuma, (unsigned)settings.ledDuty, (unsigned)settings.settleMs, settings.manualExposure ? "manual" : "auto",
           (unsigned)settings.exposure, (unsigned)settings.gain);
  return json;
}
