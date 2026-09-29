#include "net.h"

#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

static WiFiClient mqttSocket;
static PubSubClient mqtt(mqttSocket);
static String deviceId;
static String topicBase;
static void (*captureHandler)() = nullptr;
static uint32_t lastMqttAttempt = 0;
static bool wifiWasUp = false;

static String topic(const char *leaf) { return topicBase + "/" + leaf; }

static void onMqttMessage(char *t, uint8_t *, unsigned int) {
  if (topic("cmd/capture") == t && captureHandler) captureHandler();
}

void netBegin(void (*onCaptureRequest)()) {
  captureHandler = onCaptureRequest;

  uint64_t mac = ESP.getEfuseMac();  // byte 0 = first octet
  char id[24];
  snprintf(id, sizeof id, "gridtoken-%02x%02x%02x", (uint8_t)(mac >> 24), (uint8_t)(mac >> 32),
           (uint8_t)(mac >> 40));
  deviceId = id;
  topicBase = String(MQTT_TOPIC_BASE) + "/" + deviceId;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceId.c_str());
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] %s connecting to \"%s\"\n", deviceId.c_str(), WIFI_SSID);

  // Real time for TLS validity checks and payload timestamps.
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  if (mqttEnabled()) {
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(1024);  // images are streamed, this only bounds status/cmd
    mqtt.setKeepAlive(30);
  }
}

bool netWifiUp() { return WiFi.status() == WL_CONNECTED; }
const String &netDeviceId() { return deviceId; }
bool mqttEnabled() { return strlen(MQTT_HOST) > 0; }

static bool mqttConnect() {
  String avail = topic("availability");
  bool ok = strlen(MQTT_USER) > 0
                ? mqtt.connect(deviceId.c_str(), MQTT_USER, MQTT_PASSWORD, avail.c_str(), 1, true, "offline")
                : mqtt.connect(deviceId.c_str(), avail.c_str(), 1, true, "offline");
  if (!ok) return false;
  mqtt.publish(avail.c_str(), "online", true);
  mqtt.subscribe(topic("cmd/#").c_str());
  mqttPublishStatus();
  return true;
}

void netLoop() {
  bool up = netWifiUp();
  if (up != wifiWasUp) {
    wifiWasUp = up;
    if (up) {
      Serial.printf("[wifi] connected, IP %s, RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (MDNS.begin(deviceId.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mdns] http://%s.local/\n", deviceId.c_str());
      }
    } else {
      Serial.println("[wifi] disconnected");
      MDNS.end();
    }
  }
  if (!up || !mqttEnabled()) return;

  if (!mqtt.connected()) {
    if (millis() - lastMqttAttempt < 5000) return;
    lastMqttAttempt = millis();
    if (!mqttConnect()) {
      Serial.printf("[mqtt] connect to %s:%d failed, state %d\n", MQTT_HOST, MQTT_PORT, mqtt.state());
      return;
    }
    Serial.println("[mqtt] connected");
  }
  mqtt.loop();
}

String netStatusJson() {
  char json[320];
  snprintf(json, sizeof json,
           "{\"device\":\"%s\",\"fw\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"uptime_s\":%lu,"
           "\"heap\":%u,\"psram\":%u,\"time\":%lld}",
           deviceId.c_str(), FW_VERSION, WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(),
           (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
           (long long)time(nullptr));
  return json;
}

void mqttPublishStatus() {
  if (!mqtt.connected()) return;
  mqtt.publish(topic("status").c_str(), netStatusJson().c_str(), true);
}

bool mqttPublishImage(const uint8_t *buf, size_t len) {
  if (!mqtt.connected()) return false;
  // Stream the JPEG so it isn't bounded by PubSubClient's buffer.
  if (!mqtt.beginPublish(topic("image").c_str(), len, false)) return false;
  size_t sent = mqtt.write(buf, len);
  return mqtt.endPublish() && sent == len;
}
