# Roadmap

Goal: a cheap (~US$8) ESP32-CAM that clips onto a **prepaid electricity meter**,
reads the remaining-credit figure off its LCD, and reports it. First to a Node-RED
backend, and eventually as a standalone device anyone can flash and set up from a
phone, with no server, cloud or coding needed.

## Guiding principles

1. **Useful early.** Get pictures flowing, then get readings, then polish.
2. **Edge-first, backend-optional.** The end state recognises digits *on the device*.
   Node-RED, Home Assistant or anything else is an optional consumer.
3. **Prepaid-aware.** A prepaid balance behaves differently from a cumulative kWh
   counter. It goes **down** as power is used and jumps **up** on a token top-up.
   The device should understand both.
4. **Never lie.** If the reading is doubtful, report `unknown` with a reason. Don't
   report a plausible-looking wrong number.
5. **Local, open and private.** Works on the LAN with the internet down. No
   vendor cloud. MIT licensed.

## Prior art

[jomjol/AI-on-the-edge-device](https://github.com/jomjol/AI-on-the-edge-device)
already reads water/gas/electricity meters on the ESP32-CAM, using TFLite digit
classifiers. It proves the approach works on this hardware, so reuse its lessons
(and its models where the licence allows) instead of reinventing them.
GridTokenReader focuses on **prepaid-meter semantics**: balance, top-ups, burn
rate, days-left and low-credit alerts. It also targets segmented LCDs specifically
(glare, backlight timeouts, blinking icons) and aims for a simpler first-run setup.

---

## Phase 0: Skeleton ✅

- PlatformIO / Arduino-ESP32 3.x firmware for the AI-Thinker ESP32-CAM.
- Capture on an interval or on demand. The **on-board LED is the light source**
  (the meter sits in a dark, sealed compartment): PWM intensity, settle time for
  auto-exposure, and an optional fixed exposure/gain mode. Tuned live from the
  web UI and persisted in NVS.
- Two transports, each optional:
  - **HTTPS POST** of the JPEG to a webhook (e.g. a Node-RED `http in`), with basic
    auth and a pinned CA.
  - **MQTT**: JPEG streamed to `gridtoken/<id>/image`, a retained `status`, an LWT
    `availability`, and a `cmd/capture` command.
- Local web: `/` (preview + lighting sliders), `/capture`, `/settings`, `/status`, `/sd`, `/push`.
- **OTA from day one** (verified on hardware): ArduinoOTA with PBKDF2 auth
  (`pio run -e esp32cam-ota -t upload`), plus a browser/`curl` uploader at
  `/update` (basic auth, rejects non-app images). Two 1.9 MB app slots.
- Config from a git-ignored `.env` → generated header (secrets never on compiler
  command lines).
- **microSD rolling buffer** in 1-bit mode (GPIO 4 stays free for the LED): dated
  folders, oldest-first deletion to keep ≥10% free, and a write self-test that
  detects cards which silently discard writes.
- Brownout mitigation: Wi-Fi starts before the camera, and TX power is capped at 15 dBm.
- Node-RED example flow that stores every frame, with a placeholder recogniser.
- CI builds the firmware on every push.

## Phase 0.5: Store-and-forward (small, next)

- Keep SD captures marked "pending" when the HTTP/MQTT push fails, and re-send
  them oldest-first once the backend is reachable. Readings survive network and
  Node-RED outages.
- Expose the SD history in the web UI (a thumbnail strip per day), and allow
  bulk-downloading a day as a dataset.

## Phase 1: Mount, optics and a dataset (1–2 weekends)

The hardest part of meter reading is **the photo, not the model**.

- **Mounting:** a 3D-printed (or improvised) hood that fixes the camera ~8–12 cm
  from the LCD and blocks ambient light. Publish STL/STEP files in `hardware/`.
- **Focus:** the OV2640 lens is fixed-focus at ~1 m. Rotate it to focus at the
  mount distance (a known ESP32-CAM trick), or use a close-focus lens variant.
- **Lighting:** the meter sits in a **sealed, dark compartment**, so the on-board
  LED is the only light, and that's good news: the lighting is 100% controlled
  and repeatable. Tune it with the web UI sliders: lower the PWM until the digits
  aren't blown out, then switch to **manual exposure** so every frame is identical.
  If the LCD window shows a hot-spot reflection, add a diffuser (a bit of
  translucent tape or paper over the LED) or angle the camera a few degrees.
  The next step is to capture only the bright frame region and detect a
  "display off / blank" state.
- **Display behaviour:** check whether the meter shows the balance all the time,
  cycles through other screens, or blanks its backlight. Record how often each
  screen appears. This decides the capture strategy (single shot versus a burst
  that picks the best frame).
- **Dataset:** capture every 1–5 min for a few days (day, night, rain, top-up
  events). The Node-RED flow already saves every frame. Label a few hundred crops.
  This is what trains and evaluates Phases 2–3.
- **Power & Wi-Fi at the meter:** a 5 V ≥1 A supply (flash + Wi-Fi TX brownouts are
  the #1 ESP32-CAM failure), and RSSI better than −75 dBm. Else add an external
  antenna (0 Ω resistor move) or a nearby AP.

**Exit criteria:** a fixed mount, and 95%+ of frames where a human can read every
digit without zooming.

Reference meter: **SMI-810 V2**. See [meters/smi-810-v2.md](meters/smi-810-v2.md).

## Phase 2: Recognition in the backend (MVP readings) — 🚧 in shadow mode

Status (2026-10-04): `recogniser/` (7-segment sampling, no ML) is live in
**shadow mode** behind the Node-RED plausibility gate. First evaluation on 183
stored frames: 101 readable, **0 wrong values** (no upward jumps, no outliers).
All night-time failures are "unreadable"/"low confidence", and every daylight frame
was refused because outside light reflects in the cover (see HARDWARE.md, "Block
daylight"). After the camera was re-mounted ~7% closer and shifted, the reader was
reworked to align and scale to the LCD window; it reads a 5-digit balance after a
top-up correctly. Live since 2026-10-04 (`GRIDTOKEN_PUBLISH=true`), with the
breaker-based estimate (`pln_prepaid_est`) and cross-check in Node-RED. Next: fix the daylight reflections, see an LED-lit frame after a
backlight-off event, then set `GRIDTOKEN_PUBLISH=true`.


Iterate on recognition where it's cheap to change, i.e. in Node-RED/Python, not in
firmware.

- **ROI:** a fixed crop for the digit area, plus a per-digit split, defined once
  per installation and stored as JSON.
- **Recogniser options** (evaluate on the Phase 1 dataset and pick by accuracy):
  1. A classic 7-segment reader (`ssocr`, or a segment-sampling algorithm): zero
     training and fast, but fragile to glare and skew.
  2. A small CNN digit classifier (0–9 + "not a digit"), à la jomjol's `dig-class11`,
     run with TFLite in a Python sidecar called from Node-RED. **Most likely
     winner, and the same model moves on-device in Phase 3.**
  3. A vision LLM as a *labelling assistant / fallback only*. Not a dependency: it
     costs money, needs the internet, and isn't self-sufficient.
- **Plausibility filter**, which is what makes it "never lie":
  - The balance may only **decrease** slowly, bounded by main-breaker capacity ×
    elapsed time (e.g. a 25 A breaker at 230 V ≈ max 5.75 kWh/h).
  - Only accept frames showing the balance screen. On the SMI-810 the small
    display-code field identifies the screen (it shows `37` on the balance
    screen in the reference photo), so recognise that code too.
  - An **increase** is only accepted as a top-up (a jump above a threshold, confirmed
    on 2 consecutive frames).
  - Any per-digit confidence below threshold means the reading is `unknown`.
- **Outputs:** `gridtoken/<id>/reading` →
  `{kwh, confidence, ts, event?: "topup", topup_kwh?}`, an InfluxDB series,
  a Grafana panel, and low-credit alerts to Telegram/Pushover.
- **Derived metrics:** burn rate (kWh/day), **days of credit left**, top-up history,
  and a "top up by" date.

- **Backlight-off alert:** when frames arrive with `X-Capture.light = led` (the
  SMI-810 keeps its backlight off after a power cut until a key is pressed), send
  a "press a key on the meter" notification.

**Exit criteria:** 2 weeks of readings at 99%+ accuracy (or explicitly `unknown`),
and zero false top-ups.

## Phase 3: On-device recognition (edge)

- Run the Phase 2 CNN with **TFLite Micro** / ESP-NN on the ESP32. PSRAM holds the
  frame, and the model is tens of KB.
- Store the ROI + model config in **LittleFS/NVS**. Ship a default model in
  firmware, and allow uploading a new one.
- **Auto-alignment:** find the LCD bezel or reference marks and correct small
  shifts, so a bumped mount doesn't break everything.
- Publish readings (not images) by default. Send an image only on `unknown`, on a
  top-up, or once a day for audit. Less bandwidth, and more privacy.
- Keep the backend recogniser as a cross-check during rollout (shadow mode).

## Phase 4: Self-sufficient device, for everyone

Nothing to compile and nothing to edit. Flash it from the browser and set it up from
a phone.

| Area | Feature |
|---|---|
| Install | **ESP Web Tools** flasher on GitHub Pages (flash from Chrome via USB, no toolchain). Release binaries via CI. |
| Provisioning | Wi-Fi setup via a **captive-portal AP** (and/or Improv-Serial during web flashing). No `.env`, no rebuild. |
| Setup UI | On-device web app: a live preview, **drag boxes over the digits** to define the ROI, a test-read button, and flash/exposure sliders. |
| Integrations | MQTT with **Home Assistant discovery**, the generic HTTP webhook (Node-RED), a REST/JSON API, and Prometheus `/metrics`. All optional. |
| Standalone | On-device history (LittleFS ring buffer), a mini chart, days-left, and **direct alerts** (Telegram bot / ntfy / webhook) with no server at all. |
| Updates | OTA from the web UI, plus an optional check against GitHub Releases. A/B partitions with rollback. |
| Robustness | Watchdog, brownout-safe capture, Wi-Fi/MQTT backoff, a safe-mode boot after repeated crashes, NTP time. |
| Security | Web UI password, MQTT/HTTPS auth, a signed OTA option, and no open ports by default beyond the LAN UI. |
| Meter profiles | Presets per meter model: digit count, decimal position, LCD type. Community-contributed via PRs. |
| i18n | English + Bahasa Indonesia UI and docs (prepaid "token listrik" meters are the primary audience). |
| Hardware | STL mounts per meter family, a BOM, a wiring/power guide, and optional battery/deep-sleep mode. |
| Extra signals | Read the meter's **status-credit LED** (green = OK, blinking red = low) from the same frame, and optionally count the **1600 imp/kWh pulse LED** with a photodiode on a spare GPIO for real-time power (W) between readings. |

## Phase 5: Community & model lifecycle

- An opt-in "contribute a labelled crop" button, and a public dataset (no PII,
  digits only).
- A CI pipeline that retrains, evaluates against a frozen test set, and publishes
  models as release assets.
- A per-release accuracy report, per meter profile.

---

## Architecture (target)

```
 ┌──────────── ESP32-CAM ─────────────┐
 │ capture → align → ROI crop →       │  reading JSON   ┌──────────────┐
 │ TFLite digits → plausibility →     ├────────────────►│ MQTT / HA    │
 │ history + alerts + web UI          │  (image on      │ Node-RED     │
 └────────────────────────────────────┘   doubt only)   │ webhooks     │
                                                        └──────────────┘
```

## Open decisions

- **Meter model and display:** which brand, how many digits and decimals, whether
  the balance is always shown, and backlight behaviour. A photo decides a lot.
- **Transport default:** HTTPS webhook versus MQTT for the reference Node-RED
  setup. Both are supported, and Phase 4 makes MQTT + HA discovery the default for
  the public.
- **Model source:** reuse jomjol's digit models (check the licence and fit on LCD
  segments) versus training our own from the Phase 1 dataset.
