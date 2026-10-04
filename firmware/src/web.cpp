#include "web.h"

#include <SD_MMC.h>
#include <Update.h>
#include <WebServer.h>

#include "camera.h"
#include "config.h"
#include "net.h"
#include "ota.h"
#include "settings.h"
#include "storage.h"

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
<p><button onclick="snap()">Capture</button><a href="/focus">Focus aid</a><a href="/push">Push now</a><a href="/status">Status</a><a href="/sd">SD card</a><a href="/update">Firmware update</a></p>
<img id="img" alt="capture">
<fieldset><legend>Lighting &amp; exposure (saved on the device)</legend>
<label>Light <select id="light"><option value="auto">auto: backlight, LED if LCD is dark</option>
<option value="backlight">backlight only</option><option value="led">LED always</option></select></label>
<label>Dark below <input type="range" id="bl_luma" min="0" max="255"><output id="bl_luma_o"></output></label>
<p id="info"></p>
<label>LED <input type="range" id="led" min="0" max="255"><output id="led_o"></output></label>
<label>Settle ms <input type="range" id="settle" min="0" max="3000" step="50"><output id="settle_o"></output></label>
<label><input type="checkbox" id="manual"> Manual exposure (recommended once tuned: the box is lit only by the LED)</label>
<label>Exposure <input type="range" id="exposure" min="0" max="1200"><output id="exposure_o"></output></label>
<label>Gain <input type="range" id="gain" min="0" max="30"><output id="gain_o"></output></label>
<button onclick="save()">Save &amp; capture</button></fieldset>
<script>
const $=id=>document.getElementById(id);
const R=['bl_luma','led','settle','exposure','gain'];
async function snap(){const r=await fetch('/capture?t='+Date.now());$('img').src=URL.createObjectURL(await r.blob());
 $('info').textContent='lit by '+r.headers.get('X-Light')+', LCD luma '+r.headers.get('X-LCD-Luma')}
R.forEach(k=>$(k).oninput=()=>$(k+'_o').value=$(k).value);
function show(s){$('light').value=s.light;$('bl_luma').value=s.backlight_min_luma;$('led').value=s.led;$('settle').value=s.settle_ms;$('exposure').value=s.exposure;$('gain').value=s.gain;
 $('manual').checked=s.exposure_mode==='manual';R.forEach(k=>$(k+'_o').value=$(k).value)}
async function save(){const q=new URLSearchParams({light:$('light').value,bl_luma:$('bl_luma').value,led:$('led').value,settle:$('settle').value,
 aec:$('manual').checked?'manual':'auto',exposure:$('exposure').value,gain:$('gain').value});
 show(await (await fetch('/settings?'+q)).json());snap()}
fetch('/settings').then(r=>r.json()).then(show);snap();
</script></body></html>)HTML";

static long argOr(const char *name, long fallback, long lo, long hi) {
  if (!server.hasArg(name)) return fallback;
  return constrain(server.arg(name).toInt(), lo, hi);
}

static void handleCapture() {
  // Default: the configured light mode (auto = backlight, LED fallback).
  // ?led=0-255 forces that LED intensity for this shot; ?flash=0 forces no LED.
  CaptureInfo info;
  camera_fb_t *fb;
  if (server.hasArg("led") || server.arg("flash") == "0") {
    uint8_t duty = server.arg("flash") == "0" ? 0 : argOr("led", settings.ledDuty, 0, 255);
    fb = cameraCapture(duty);
    info.usedLed = duty > 0;
    info.lcdLuma = cameraRegionLuma(fb);
  } else {
    fb = cameraCaptureAuto(info);
  }
  if (!fb) {
    server.send(503, "text/plain", "capture failed");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Light", info.usedLed ? "led" : "backlight");
  server.sendHeader("X-LCD-Luma", String(info.lcdLuma));
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// GET /settings                  -> current settings JSON
// GET /settings?light=auto|backlight|led&bl_luma=..&led=..&settle=..&aec=auto|manual&exposure=..&gain=..
//                                -> update + save
static void handleSettings() {
  if (server.args() > 0) {
    if (server.hasArg("light")) {
      String m = server.arg("light");
      settings.lightMode = m == "backlight" ? LIGHT_BACKLIGHT : m == "led" ? LIGHT_LED : LIGHT_AUTO;
    }
    settings.backlightMinLuma = argOr("bl_luma", settings.backlightMinLuma, 0, 255);
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

// Focus aid: live preview + sharpness score (variance of the Laplacian over the
// middle of the frame, computed in the browser). Turn the lens until it peaks.
static const char FOCUS_HTML[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GridTokenReader focus</title>
<style>body{font-family:system-ui,sans-serif;margin:16px;max-width:840px}img{width:100%;border-radius:8px;background:#222}
#bar{height:18px;background:#8883;border-radius:9px;position:relative;margin:8px 0}#fill{height:100%;background:#2a8;border-radius:9px;width:0}
#best{position:absolute;top:-3px;width:3px;height:24px;background:#e33}.n{font:600 28px system-ui}</style></head><body>
<h1>Focus aid</h1>
<p>Turn the lens a little at a time and wait for the next picture. Stop where the score peaks
(red mark = best so far). <a href="/">Back</a></p>
<p>Sharpness <span class="n" id="s">-</span> &nbsp; best <span class="n" id="b">-</span>
<button onclick="best=0">Reset best</button></p>
<div id="bar"><div id="fill"></div><div id="best"></div></div>
<img id="img" alt="preview"><canvas id="c" width="400" height="300" hidden></canvas>
<script>
let best=0;const $=id=>document.getElementById(id),ctx=$('c').getContext('2d',{willReadFrequently:true});
async function tick(){try{
 const r=await fetch('/capture?t='+Date.now());const blob=await r.blob();$('img').src=URL.createObjectURL(blob);
 const bmp=await createImageBitmap(blob);ctx.drawImage(bmp,0,0,400,300);
 const d=ctx.getImageData(40,60,320,180).data,w=320,h=180,g=new Float32Array(w*h);
 for(let i=0;i<w*h;i++)g[i]=(d[4*i]+2*d[4*i+1]+d[4*i+2])/4;
 let sum=0,sq=0,n=0;for(let y=1;y<h-1;y++)for(let x=1;x<w-1;x++){const i=y*w+x,l=4*g[i]-g[i-1]-g[i+1]-g[i-w]-g[i+w];sum+=l;sq+=l*l;n++}
 const v=sq/n-(sum/n)**2;best=Math.max(best,v);
 $('s').textContent=v.toFixed(1);$('b').textContent=best.toFixed(1);
 $('fill').style.width=Math.min(100,100*v/(best*1.15))+'%';$('best').style.left=Math.min(100,100/1.15)+'%';
}catch(e){$('s').textContent='...'}setTimeout(tick,400)}
tick();
</script></body></html>)HTML";

static const char UPDATE_HTML[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GridTokenReader update</title><style>body{font-family:system-ui,sans-serif;margin:16px;max-width:640px}</style></head>
<body><h1>Firmware update</h1><p>Upload <code>firmware.bin</code> (not <code>firmware.factory.bin</code>).</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin" required> <button>Upload &amp; reboot</button></form>
<p><a href="/">Back</a></p></body></html>)HTML";

static bool uploadAuthorized = false;
static bool uploadRejected = false;
static uint8_t uploadHead[36];  // image header, gathered across (possibly tiny) first chunks
static size_t uploadHeadLen = 0;

static bool checkOtaAuth() {
  if (!otaEnabled()) {
    server.send(403, "text/plain", "OTA disabled: set OTA_PASSWORD in .env");
    return false;
  }
  if (!server.authenticate(OTA_WEB_USER, OTA_PASSWORD)) {
    server.requestAuthentication(BASIC_AUTH, "GridTokenReader");
    return false;
  }
  return true;
}

// Streams the uploaded image straight into the inactive OTA partition.
static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadAuthorized = otaEnabled() && server.authenticate(OTA_WEB_USER, OTA_PASSWORD);
    uploadRejected = false;
    uploadHeadLen = 0;
    if (!uploadAuthorized) return;
    otaSetInProgress(true);
    Serial.printf("[ota] web upload: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
    return;
  }
  if (!uploadAuthorized || uploadRejected) return;

  if (up.status == UPLOAD_FILE_WRITE) {
    // Writing into the inactive slot is harmless; the boot partition only
    // changes in Update.end(). So validate once the header is complete.
    if (uploadHeadLen < sizeof uploadHead) {
      size_t n = min(sizeof uploadHead - uploadHeadLen, up.currentSize);
      memcpy(uploadHead + uploadHeadLen, up.buf, n);
      uploadHeadLen += n;
      if (uploadHeadLen == sizeof uploadHead && !otaLooksLikeAppImage(uploadHead, uploadHeadLen)) {
        Serial.println("[ota] rejected: not an application image (factory.bin?)");
        uploadRejected = true;
        Update.abort();
        return;
      }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadHeadLen < sizeof uploadHead) {
      uploadRejected = true;
      Update.abort();
    } else if (Update.end(true)) Serial.printf("[ota] web upload ok, %u bytes\n", (unsigned)up.totalSize);
    else Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

static void handleUpdateDone() {
  if (!uploadAuthorized) {
    checkOtaAuth();
    return;
  }
  bool ok = !uploadRejected && !Update.hasError() && Update.isFinished();
  server.sendHeader("Connection", "close");
  if (ok) {
    server.send(200, "text/plain", "Update OK - rebooting");
    delay(500);
    ESP.restart();
  }
  server.send(400, "text/plain",
              uploadRejected ? "Rejected: upload firmware.bin, not factory.bin"
                             : String("Update failed: ") + Update.errorString());
  otaSetInProgress(false);
}

void webBegin(void (*onPushRequest)()) {
  pushHandler = onPushRequest;
  server.on("/", HTTP_GET, [] { server.send(200, "text/html", INDEX_HTML); });
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/focus", HTTP_GET, [] { server.send(200, "text/html", FOCUS_HTML); });
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/status", HTTP_GET, [] { server.send(200, "application/json", netStatusJson()); });
  server.on("/update", HTTP_GET, [] {
    if (checkOtaAuth()) server.send(200, "text/html", UPDATE_HTML);
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/sd", HTTP_GET, [] { server.send(200, "application/json", storageStatusJson()); });
  server.on("/sd/list", HTTP_GET, [] { server.send(200, "application/json", storageListJson(server.arg("day"))); });
  server.on("/sd/file", HTTP_GET, [] {
    String path = server.arg("path");
    File f = storageReady() && storageIsCapturePath(path) ? SD_MMC.open(path) : File();
    if (!f || f.isDirectory()) {
      server.send(404, "text/plain", "not found");
      return;
    }
    server.streamFile(f, "image/jpeg");
    f.close();
  });
  server.on("/push", HTTP_GET, [] {
    if (pushHandler) pushHandler();
    server.send(202, "text/plain", "push queued");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }
