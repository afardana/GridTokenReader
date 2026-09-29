# Node-RED example flow

`gridtoken-flow.json` adds a **GridTokenReader** tab that:

1. Accepts frames via **HTTP** `POST /gridtoken/ingest` (body = JPEG, header
   `X-Device-Id`) and/or **MQTT** `gridtoken/+/image`.
2. Saves every frame to `$GRIDTOKEN_DIR/<device>/<timestamp>.jpg` (default
   `/tmp/gridtoken`). This builds the Phase 1 dataset.
3. Passes the frame to a placeholder **recognise** function (Phase 2).
4. Shows device `status` messages and has an inject button that sends
   `cmd/capture` over MQTT.

## Import

Menu → Import → select `gridtoken-flow.json` → Deploy. Then:

- Set a persistent `GRIDTOKEN_DIR` (tab → Edit → Environment variables).
- If you only use HTTP, you can delete the MQTT nodes and the `local broker` config.
- If you only use MQTT, point `local broker` at your broker.

## Firmware settings for HTTP ingest

```c
#define INGEST_URL      "https://nodered.example.com/gridtoken/ingest"
#define INGEST_USER     "..."   // settings.js httpNodeAuth user
#define INGEST_PASSWORD "..."
```

If Node-RED sits behind nginx, a frame is ~40–120 KB, so the default
`client_max_body_size 1m` is fine.
