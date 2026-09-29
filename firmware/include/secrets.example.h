#pragma once

// Copy to include/secrets.h (git-ignored) and fill in.
// Leave a transport empty ("") to disable it.

#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"

// --- Transport A: MQTT (Node-RED, Home Assistant, anything MQTT) ------------
#define MQTT_HOST      ""            // e.g. "192.168.1.10"
#define MQTT_PORT      1883
#define MQTT_USER      ""
#define MQTT_PASSWORD  ""

// --- Transport B: HTTP(S) POST of each JPEG to a webhook ---------------------
// e.g. a Node-RED "http in" node: "https://nodered.example.com/gridtoken/ingest"
#define INGEST_URL       ""
#define INGEST_USER      ""          // HTTP basic auth (Node-RED httpNodeAuth)
#define INGEST_PASSWORD  ""
// HTTPS trusts ISRG Root X1 (Let's Encrypt) by default. For another CA,
// define INGEST_CA_CERT as a PEM string here.
