# GridTokenReader

**Read your prepaid electricity meter with a US$8 camera.**

GridTokenReader turns an **ESP32-CAM** into a meter reader. It clips onto a prepaid
electricity meter, photographs the LCD, and reports the **remaining credit (kWh)**
to Node-RED, Home Assistant or anything that speaks MQTT/HTTP. Eventually it will do
this fully on-device, with no server needed.

> **Status: Phase 0 (skeleton).** The device captures and pushes frames. Digit
> recognition is next. See the [roadmap](docs/ROADMAP.md).

## What works today

- Periodic and on-demand capture (flash LED, stale-frame discard, SVGA JPEG)
- Push via **HTTPS webhook** (basic auth, pinned Let's Encrypt root) and/or **MQTT**
- Local web UI: live preview `/`, `/capture`, `/status`, `/push`
- A [Node-RED flow](node-red/) that ingests and stores frames for dataset building

## Quick start

Requirements: an AI-Thinker ESP32-CAM (with PSRAM) + an ESP32-CAM-MB USB board,
and [PlatformIO](https://platformio.org/install/cli).

```bash
cd firmware
cp include/secrets.example.h include/secrets.h   # set Wi-Fi + MQTT and/or INGEST_URL
pio run -t upload
pio device monitor
```

Then open `http://gridtoken-xxxxxx.local/` or the IP printed on the serial console.

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
docs/         Roadmap, hardware notes
```

## Contributing

Issues and PRs are welcome, especially **photos of different prepaid meter models**,
mounts, and labelled digit crops. See [docs/ROADMAP.md](docs/ROADMAP.md).

## License

[MIT](LICENSE)
