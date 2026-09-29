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

// Extracts host and port from "scheme://host[:port]/path".
static bool parseUrl(const String &url, String &host, uint16_t &port) {
  int start = url.indexOf("://");
  if (start < 0) return false;
  start += 3;
  int end = url.indexOf('/', start);
  String authority = url.substring(start, end < 0 ? url.length() : end);
  int colon = authority.indexOf(':');
  host = colon < 0 ? authority : authority.substring(0, colon);
  port = colon < 0 ? (url.startsWith("https") ? 443 : 80) : authority.substring(colon + 1).toInt();
  return host.length() > 0;
}

bool ingestPostImage(const uint8_t *buf, size_t len, const String &captureJson) {
  if (!ingestEnabled() || !netWifiUp()) return false;

  const String url = INGEST_URL;
  const bool https = url.startsWith("https://");
  WiFiClientSecure tls;
  WiFiClient plain;
  WiFiClient &client = https ? static_cast<WiFiClient &>(tls) : plain;
  if (https) tls.setCACert(INGEST_CA_CERT);

  // Optional LAN shortcut: open the socket to INGEST_CONNECT_IP ourselves (TLS
  // SNI + cert check still use the URL host); HTTPClient then reuses it.
  IPAddress connectIp;
  if (strlen(INGEST_CONNECT_IP) > 0 && connectIp.fromString(INGEST_CONNECT_IP)) {
    String host;
    uint16_t port;
    if (!parseUrl(url, host, port)) {
      Serial.println("[ingest] bad INGEST_URL");
      return false;
    }
    int ok = https ? tls.connect(connectIp, port, host.c_str(), INGEST_CA_CERT, nullptr, nullptr)
                   : plain.connect(connectIp, port);
    if (!ok) {
      Serial.printf("[ingest] connect %s:%u (%s) failed\n", INGEST_CONNECT_IP, port, host.c_str());
      return false;
    }
  }

  HTTPClient http;
  http.setReuse(true);
  if (!http.begin(client, url)) {
    Serial.println("[ingest] bad INGEST_URL");
    return false;
  }
  http.setTimeout(15000);
  if (strlen(INGEST_TOKEN) > 0) http.addHeader("Authorization", String("Bearer ") + INGEST_TOKEN);
  else if (strlen(INGEST_USER) > 0) http.setAuthorization(INGEST_USER, INGEST_PASSWORD);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Device-Id", netDeviceId());  // informational; a token-checking proxy overrides it
  http.addHeader("X-Firmware", FW_VERSION);
  http.addHeader("X-Device-Status", netStatusJson());
  http.addHeader("X-Capture", captureJson);

  int code = http.POST(const_cast<uint8_t *>(buf), len);
  http.end();
  if (code < 200 || code >= 300) {
    Serial.printf("[ingest] POST failed: %d %s\n", code, code < 0 ? HTTPClient::errorToString(code).c_str() : "");
    return false;
  }
  return true;
}
