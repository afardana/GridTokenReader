#pragma once

#include <Arduino.h>

// microSD rolling capture buffer.
//
// The card runs in 1-bit SD mode (CLK 14, CMD 15, D0 2): 4-bit mode would take
// GPIO 4 as DAT1, but that pin drives the flash LED we need in the dark meter box.
//
// Layout: /captures/<YYYY-MM-DD>/<YYYYMMDD>T<HHMMSS>Z.jpg (UTC), or
//         /captures/0000-unsynced/... before NTP time is known.
// Retention: before each save, the oldest captures are deleted until at least
// SD_MIN_FREE_PCT of the card is free, so the card never fills up.
bool storageBegin();
bool storageReady();
bool storageSaveJpeg(const uint8_t *buf, size_t len);
String storageStatusJson();

// JSON list of day folders (day == "") or of the files in one day folder.
String storageListJson(const String &day);
// True for paths safe to serve: under the capture dir, no "..".
bool storageIsCapturePath(const String &path);
