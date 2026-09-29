# GridTokenReader

**Read your prepaid electricity meter with a US$8 camera.**

GridTokenReader turns an **ESP32-CAM** into a meter reader. It clips onto a prepaid
electricity meter, photographs the LCD, and reports the **remaining credit (kWh)**
to Node-RED, Home Assistant or anything that speaks MQTT/HTTP. Eventually it will do
this fully on-device, with no server needed.

> **Status: Phase 0 (skeleton).** The device captures and pushes frames. Digit
> recognition is next. See the [roadmap](docs/ROADMAP.md).

## What works today

- Periodic and on-demand capture, SVGA JPEG
- **On-board LED as the light source** for dark/sealed meter compartments: PWM
  intensity, AEC settle time, and optional fixed exposure, all tunable live from
  the web UI and saved on the device
- Push via **HTTPS webhook** (basic auth, pinned Let's Encrypt root, optional LAN
  IP override) and/or **MQTT**
- **OTA updates from day one**: `pio run -e esp32cam-ota -t upload` over Wi-Fi, or
  upload `firmware.bin` in the browser at `/update`. Both are password-protected, and
  the uploader rejects `factory.bin`/bootloader images.
- **microSD rolling buffer**: every capture is saved in dated folders, and the oldest
  are deleted automatically to keep ≥10% free. A self-test refuses cards that
  don't really store writes.
- Local web UI: preview + lighting sliders `/`, `/capture`, `/settings`, `/status`,
  `/sd`, `/push`, `/update`
- A [Node-RED flow](node-red/) that ingests and stores frames for dataset building

## Quick start

Requirements: an AI-Thinker ESP32-CAM (with PSRAM) + an ESP32-CAM-MB USB board,
and [PlatformIO](https://platformio.org/install/cli).

```bash
cp .env.example .env          # Wi-Fi, MQTT and/or INGEST_URL, OTA_PASSWORD (git-ignored)
cd firmware
pio run -t upload             # first flash over USB
pio device monitor
```

Then open `http://gridtoken-xxxxxx.local/` or the IP printed on the serial console.
After that, update over Wi-Fi:

```bash
pio run -e esp32cam-ota -t upload      # uses OTA_HOST + OTA_PASSWORD from .env
```

or upload `firmware/.pio/build/esp32cam/firmware.bin` at `http://<device>/update`
(user `admin`, password `OTA_PASSWORD`).

## MQTT topics

| Topic | Direction | Payload |
|---|---|---|
| `gridtoken/<id>/availability` | device → | `online` / `offline` (retained, LWT) |
| `gridtoken/<id>/status` | device → | JSON health (retained) |
| `gridtoken/<id>/image` | device → | JPEG bytes |
| `gridtoken/<id>/cmd/capture` | → device | any payload: capture and push now |
| `gridtoken/<id>/reading` | device → | *(Phase 2+)* `{kwh, confidence, ts, event?}` |

## Repository layout

```
firmware/     PlatformIO project (Arduino-ESP32 3.x)
node-red/     Example Node-RED flow
docs/         Roadmap, hardware notes, meter profiles
.env.example  Build-time configuration template (copy to .env)
```

## Contributing

Issues and PRs are welcome, especially **photos of different prepaid meter models**,
mounts, and labelled digit crops. See [docs/ROADMAP.md](docs/ROADMAP.md).

## License

[MIT](LICENSE)
