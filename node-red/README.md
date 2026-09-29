# Node-RED flow

`gridtoken-flow.json` adds a **GridTokenReader** tab (regenerate or customise with
`python3 build-flow.py <influx-config-id|new> <org> <bucket>`):

| Endpoint | Does |
|---|---|
| `POST /gridtoken/ingest` (JPEG) | Validates it's a JPEG from a known device, saves `$GRIDTOKEN_FRAMES/<device>/<YYYY-MM-DD>/<HHMMSS>.jpg` + `latest.jpg`, writes device health to InfluxDB `gridtoken_ingest`, and hands the frame to the recogniser hook (Phase 2) |
| `POST /gridtoken/reading` (JSON `{"kwh": …}`) | Writes InfluxDB `pln_prepaid` (`kwh`, `confidence`; tags `device`, `source`) |

A daily job prunes frames older than 30 days.

The device identity is read from `X-Device-Id`, which **must be set by a trusted
reverse proxy** from the device's token. See [../docs/NGINX.md](../docs/NGINX.md).
Don't expose these endpoints without it.

## Import

Menu → Import → `gridtoken-flow.json` → Deploy. Then open the **InfluxDB (set token)**
config node, add your token, and set org/bucket on the *InfluxDB* batch node (or
generate the flow with your existing config node id).

MQTT users: the firmware also publishes `gridtoken/<id>/image` (JPEG) and `status`
(see the main README). Wire an `mqtt in` node into *validate frame*'s successors
if you prefer MQTT.
