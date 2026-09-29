#include "ingest.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "net.h"
#include "root_ca.h"

#ifndef INGEST_CA_CERT
#define INGEST_CA_CERT ROOT_CA_ISRG_X1
#endif

bool ingestEnabled() { return strlen(INGEST_URL) > 0; }

bool ingestPostImage(const uint8_t *buf, size_t len) {
  if (!ingestEnabled() || !netWifiUp()) return false;

  WiFiClientSecure tls;
  WiFiClient plain;
  HTTPClient http;
  bool https = strncmp(INGEST_URL, "https://", 8) == 0;
  if (https) tls.setCACert(INGEST_CA_CERT);
  if (!(https ? http.begin(tls, INGEST_URL) : http.begin(plain, INGEST_URL))) {
    Serial.println("[ingest] bad INGEST_URL");
    return false;
  }

  http.setTimeout(15000);
  if (strlen(INGEST_USER) > 0) http.setAuthorization(INGEST_USER, INGEST_PASSWORD);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Device-Id", netDeviceId());
  http.addHeader("X-Firmware", FW_VERSION);

  int code = http.POST(const_cast<uint8_t *>(buf), len);
  http.end();
  if (code < 200 || code >= 300) {
    Serial.printf("[ingest] POST failed: %d %s\n", code, code < 0 ? HTTPClient::errorToString(code).c_str() : "");
    return false;
  }
  return true;
}
