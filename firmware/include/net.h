#pragma once

#include <Arduino.h>

// Wi-Fi + MQTT. Topics live under MQTT_TOPIC_BASE/<device-id>/:
//   availability  "online" / "offline" (retained, LWT)
//   status        JSON health snapshot (retained)
//   image         raw JPEG bytes
//   cmd/capture   any payload -> capture and push now
void netBegin(void (*onCaptureRequest)());
void netLoop();
bool netWifiUp();
const String &netDeviceId();
String netStatusJson();

bool mqttEnabled();
bool mqttPublishImage(const uint8_t *buf, size_t len);
void mqttPublishStatus();
