#include "web.h"

#include <WebServer.h>

#include "camera.h"
#include "config.h"
#include "net.h"
#include "settings.h"

static WebServer server(80);
static void (*pushHandler)() = nullptr;

static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GridTokenReader</title>
<style>body{font-family:system-ui,sans-serif;margin:16px;max-width:840px}img{width:100%;border-radius:8px;background:#222}
a,button{margin-right:12px}fieldset{margin:12px 0;border:1px solid #8884;border-radius:8px}
label{display:flex;gap:8px;align-items:center;margin:6px 0}input[type=range]{flex:1}output{min-width:3.5em;text-align:right}</style>
</head><body>
<h1>GridTokenReader</h1>
<p><button onclick="snap()">Capture</button><a href="/push">Push now</a><a href="/status">Status</a></p>
<img id="img" alt="capture">
<fieldset><legend>Lighting &amp; exposure (saved on the device)</legend>
<label>LED <input type="range" id="led" min="0" max="255"><output id="led_o"></output></label>
<label>Settle ms <input type="range" id="settle" min="0" max="3000" step="50"><output id="settle_o"></output></label>
<label><input type="checkbox" id="manual"> Manual exposure (recommended once tuned: the box is lit only by the LED)</label>
<label>Exposure <input type="range" id="exposure" min="0" max="1200"><output id="exposure_o"></output></label>
<label>Gain <input type="range" id="gain" min="0" max="30"><output id="gain_o"></output></label>
<button onclick="save()">Save &amp; capture</button></fieldset>
<script>
const $=id=>document.getElementById(id);
function snap(){$('img').src='/capture?t='+Date.now()}
['led','settle','exposure','gain'].forEach(k=>$(k).oninput=()=>$(k+'_o').value=$(k).value);
function show(s){$('led').value=s.led;$('settle').value=s.settle_ms;$('exposure').value=s.exposure;$('gain').value=s.gain;
 $('manual').checked=s.exposure_mode==='manual';['led','settle','exposure','gain'].forEach(k=>$(k+'_o').value=$(k).value)}
async function save(){const q=new URLSearchParams({led:$('led').value,settle:$('settle').value,
 aec:$('manual').checked?'manual':'auto',exposure:$('exposure').value,gain:$('gain').value});
 show(await (await fetch('/settings?'+q)).json());snap()}
fetch('/settings').then(r=>r.json()).then(show);snap();
</script></body></html>)HTML";

static long argOr(const char *name, long fallback, long lo, long hi) {
  if (!server.hasArg(name)) return fallback;
  return constrain(server.arg(name).toInt(), lo, hi);
}

static void handleCapture() {
  // ?led=0-255 overrides the saved intensity for this shot (?flash=0 = no LED).
  uint8_t duty = server.arg("flash") == "0" ? 0 : argOr("led", settings.ledDuty, 0, 255);
  camera_fb_t *fb = cameraCapture(duty);
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

// GET /settings                  -> current settings JSON
// GET /settings?led=..&settle=..&aec=auto|manual&exposure=..&gain=..  -> update + save
static void handleSettings() {
  if (server.args() > 0) {
    settings.ledDuty = argOr("led", settings.ledDuty, 0, 255);
    settings.settleMs = argOr("settle", settings.settleMs, 0, 5000);
    if (server.hasArg("aec")) settings.manualExposure = server.arg("aec") == "manual";
    settings.exposure = argOr("exposure", settings.exposure, 0, 1200);
    settings.gain = argOr("gain", settings.gain, 0, 30);
    settingsSave();
    cameraApplySettings();
  }
  server.send(200, "application/json", settingsJson());
}

void webBegin(void (*onPushRequest)()) {
  pushHandler = onPushRequest;
  server.on("/", HTTP_GET, [] { server.send(200, "text/html", INDEX_HTML); });
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/status", HTTP_GET, [] { server.send(200, "application/json", netStatusJson()); });
  server.on("/push", HTTP_GET, [] {
    if (pushHandler) pushHandler();
    server.send(202, "text/plain", "push queued");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }
