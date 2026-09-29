#pragma once

// Local HTTP API on port 80:
//   GET /             tiny preview page
//   GET /capture      fresh JPEG lit by the flash LED (?led=0-255 override, ?flash=0 off)
//   GET /settings     LED intensity / settle / exposure (query params update + persist)
//   GET /status       health JSON
//   GET /push         capture now and push via the configured transports
//   GET /sd           SD card status; /sd/list[?day=YYYY-MM-DD]; /sd/file?path=/captures/...
//   GET|POST /update  browser firmware upload (basic auth OTA_WEB_USER / OTA_PASSWORD)
void webBegin(void (*onPushRequest)());
void webLoop();
