#include "storage.h"

#include <SD_MMC.h>
#include <errno.h>
#include <time.h>

#include "config.h"

static bool mounted = false;
static String sdError;
static uint32_t savedCount = 0, deletedCount = 0, failedCount = 0;
static String lastSaved;
static uint32_t bootId = 0;

static constexpr uint64_t MB = 1024ULL * 1024ULL;

bool storageReady() { return mounted; }

bool storageBegin() {
  bootId = esp_random();
  if (!SD_MMC.begin("/sdcard", /*mode1bit=*/true)) {
    sdError = "no card or mount failed (needs FAT32; exFAT cards >32 GB must be reformatted)";
    Serial.printf("[sd] %s\n", sdError.c_str());
    return false;
  }
  uint8_t type = SD_MMC.cardType();
  if (type == CARD_NONE) {
    sdError = "no card";
    Serial.println("[sd] no card");
    SD_MMC.end();
    return false;
  }
  // Write self-test. Worn-out / counterfeit cards can fall into a state where
  // writes are acknowledged but silently discarded; FATFS caching then makes a
  // plain write+stat look fine. Require the probe to show up in a real
  // directory listing (which re-reads the card) before trusting it.
  char probeName[32];
  snprintf(probeName, sizeof probeName, "/gt-probe-%08lx.txt", (unsigned long)bootId);
  File probe = SD_MMC.open(probeName, FILE_WRITE);
  bool wrote = probe && probe.print("ok") == 2;
  if (probe) probe.close();
  bool listed = false;
  File root = SD_MMC.open("/");
  bool isDir = false;
  for (String p = root.getNextFileName(&isDir); wrote && p.length(); p = root.getNextFileName(&isDir)) {
    if (p == probeName) listed = true;
  }
  root.close();
  SD_MMC.remove(probeName);
  if (!listed) {
    sdError = wrote ? "card accepts writes but does not keep them (faulty, worn-out or counterfeit card) - replace it"
                    : String("card is not writable (errno ") + errno + ": " + strerror(errno) + ")";
    Serial.printf("[sd] %s\n", sdError.c_str());
    SD_MMC.end();
    return false;
  }
  mounted = true;
  if (!SD_MMC.exists(SD_CAPTURE_DIR) && !SD_MMC.mkdir(SD_CAPTURE_DIR)) {
    Serial.printf("[sd] mkdir %s failed (errno %d: %s)\n", SD_CAPTURE_DIR, errno, strerror(errno));
  }
  Serial.printf("[sd] %s card %llu MB, FS %llu MB, used %llu MB\n",
                type == CARD_MMC ? "MMC" : type == CARD_SDHC ? "SDHC" : "SD", SD_MMC.cardSize() / MB,
                SD_MMC.totalBytes() / MB, SD_MMC.usedBytes() / MB);
  return true;
}

// Lexicographically smallest entry (full path) of the wanted kind in `dir`.
// Names are timestamps, so smallest == oldest.
static String oldestEntry(const String &dir, bool wantDir) {
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) return "";
  String best;
  bool isDir = false;
  for (String p = d.getNextFileName(&isDir); p.length(); p = d.getNextFileName(&isDir)) {
    if (isDir == wantDir && (best.isEmpty() || p < best)) best = p;
  }
  return best;
}

static bool deleteOldestCapture() {
  String day = oldestEntry(SD_CAPTURE_DIR, true);
  if (day.isEmpty()) return false;
  String file = oldestEntry(day, false);
  if (file.isEmpty()) return SD_MMC.rmdir(day);  // empty day folder
  if (!SD_MMC.remove(file)) return false;
  deletedCount++;
  return true;
}

// Delete oldest captures until `incoming` bytes fit with SD_MIN_FREE_PCT to spare.
static void enforceRetention(size_t incoming) {
  uint64_t total = SD_MMC.totalBytes();
  uint64_t reserve = total * SD_MIN_FREE_PCT / 100 + incoming;
  for (int i = 0; i < SD_MAX_DELETES_PER_SAVE && total - SD_MMC.usedBytes() < reserve; i++) {
    if (!deleteOldestCapture()) break;
  }
}

bool storageSaveJpeg(const uint8_t *buf, size_t len) {
  if (!mounted) return false;
  enforceRetention(len);

  char dir[48], path[96];
  time_t now = time(nullptr);
  if (now > 1700000000) {  // NTP synced
    struct tm t;
    gmtime_r(&now, &t);
    snprintf(dir, sizeof dir, SD_CAPTURE_DIR "/%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    snprintf(path, sizeof path, "%s/%04d%02d%02dT%02d%02d%02dZ.jpg", dir, t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    snprintf(dir, sizeof dir, SD_CAPTURE_DIR "/0000-unsynced");
    snprintf(path, sizeof path, "%s/%08lx-%010lu.jpg", dir, (unsigned long)bootId, (unsigned long)millis());
  }
  if (!SD_MMC.exists(dir) && !SD_MMC.mkdir(dir)) {
    Serial.printf("[sd] mkdir %s failed (errno %d: %s)\n", dir, errno, strerror(errno));
  }

  File f = SD_MMC.open(path, FILE_WRITE);
  size_t written = f ? f.write(buf, len) : 0;
  if (f) f.close();
  if (written != len) {
    SD_MMC.remove(path);
    failedCount++;
    Serial.printf("[sd] write failed: %s (errno %d: %s)\n", path, errno, strerror(errno));
    return false;
  }
  savedCount++;
  lastSaved = path;
  return true;
}

String storageStatusJson() {
  if (!mounted) return "{\"mounted\":false,\"error\":\"" + sdError + "\"}";
  uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  char json[256];
  snprintf(json, sizeof json,
           "{\"mounted\":true,\"total_mb\":%llu,\"used_mb\":%llu,\"free_pct\":%u,\"min_free_pct\":%u,"
           "\"saved\":%lu,\"deleted\":%lu,\"failed\":%lu,\"last\":\"%s\"}",
           total / MB, used / MB, (unsigned)(total ? (total - used) * 100 / total : 0), (unsigned)SD_MIN_FREE_PCT,
           (unsigned long)savedCount, (unsigned long)deletedCount, (unsigned long)failedCount, lastSaved.c_str());
  return json;
}

String storageListJson(const String &day) {
  if (!mounted) return "[]";
  String dir = day.isEmpty() ? String(SD_CAPTURE_DIR) : String(SD_CAPTURE_DIR) + "/" + day;
  if (!storageIsCapturePath(dir)) return "[]";
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) return "[]";
  String out = "[";
  bool isDir = false;
  for (String p = d.getNextFileName(&isDir); p.length(); p = d.getNextFileName(&isDir)) {
    if (isDir != day.isEmpty()) continue;  // days: folders; files: jpgs
    if (out.length() > 1) out += ",";
    out += "\"" + p.substring(p.lastIndexOf('/') + 1) + "\"";
  }
  return out + "]";
}

bool storageIsCapturePath(const String &path) {
  return path.startsWith(SD_CAPTURE_DIR) && path.indexOf("..") < 0 && path.indexOf('"') < 0;
}
