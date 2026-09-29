#include "web.h"

#include <WebServer.h>

#include "camera.h"
#include "config.h"
#include "net.h"

static WebServer server(80);
static void (*pushHandler)() = nullptr;

static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GridTokenReader</title>
<style>body{font-family:system-ui,sans-serif;margin:16px;max-width:840px}img{width:100%;border-radius:8px;background:#222}
a{margin-right:12px}</style></head><body>
<h1>GridTokenReader</h1>
<p><a href="#" onclick="snap(1)">Capture (flash)</a><a href="#" onclick="snap(0)">Capture (no flash)</a>
<a href="/push">Push now</a><a href="/status">Status</a></p>
<img id="img" alt="capture">
<script>function snap(f){document.getElementById('img').src='/capture?flash='+f+'&t='+Date.now();return false}snap(1)</script>
</body></html>)HTML";

static void handleCapture() {
  bool flash = server.arg("flash") != "0";
  camera_fb_t *fb = cameraCapture(flash);
  if (!fb) {
    server.send(503, "text/plain", "capture failed");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void webBegin(void (*onPushRequest)()) {
  pushHandler = onPushRequest;
  server.on("/", HTTP_GET, [] { server.send(200, "text/html", INDEX_HTML); });
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/status", HTTP_GET, [] { server.send(200, "application/json", netStatusJson()); });
  server.on("/push", HTTP_GET, [] {
    if (pushHandler) pushHandler();
    server.send(202, "text/plain", "push queued");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }
