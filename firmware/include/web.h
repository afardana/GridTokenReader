#pragma once

// Local HTTP API on port 80:
//   GET /             tiny preview page
//   GET /capture      fresh JPEG (?flash=0 to keep the LED off)
//   GET /status       health JSON
//   GET /push         capture now and push via the configured transports
void webBegin(void (*onPushRequest)());
void webLoop();
