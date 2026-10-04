# Node-RED flow

`gridtoken-flow.json` adds a **GridTokenReader** tab (regenerate or customise with
`python3 build-flow.py <influx-config-id|new> <org> <bucket>`):

| Endpoint | Does |
|---|---|
| `POST /gridtoken/ingest` (JPEG) | Validates it's a JPEG from a known device, saves `$GRIDTOKEN_FRAMES/<device>/<YYYY-MM-DD>/<HHMMSS>.jpg` + `latest.jpg`, writes device health to InfluxDB `gridtoken_ingest`, and hands the frame to the recogniser hook (Phase 2) |
| `POST /gridtoken/reading` (JSON `{"kwh": …}`) | Writes InfluxDB `pln_prepaid` (`kwh`, `confidence`; tags `device`, `source`) |

Each saved frame goes through the [recogniser](../recogniser/) and a plausibility
gate; every result is logged to `gridtoken_recognition`. With a grid-power series
in InfluxDB (tab env `GRIDTOKEN_POWER_MEASUREMENT`, e.g. a smart breaker's watts):

- camera readings are **cross-checked** against *last reading − energy used since*
  (within `GRIDTOKEN_EST_TOL_KWH` + `GRIDTOKEN_EST_TOL_FRAC` × energy used), which
  catches single-digit misreads; a large rise is a top-up, confirmed by the next frame;
- every 5 minutes the estimate is stored as `pln_prepaid_est` (`kwh`, `rupiah`,
  `days_left` from the 7-day average, `anchor_age_h`, …), so the balance has a
  history even when the camera can't read the display;
- Telegram alerts: top-up, backlight off/on, and low credit (`GRIDTOKEN_LOW_KWH`).

Without a power series the gate falls back to "the balance only falls, at most at
`GRIDTOKEN_MAX_KW`". A daily job prunes frames older than 30 days.

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
