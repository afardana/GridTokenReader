# Token gate in front of Node-RED (nginx)

Devices authenticate with a **per-device bearer token**. nginx checks it, stamps
the device id that belongs to the token, and adds Node-RED's `httpNodeAuth` itself.
Devices never hold your Node-RED password, a leaked token can be revoked on its
own, and a device can't claim to be another device.

```
ESP32-CAM ──HTTPS, Authorization: Bearer <token>──► nginx ──(X-Device-Id, Basic httpNodeAuth)──► Node-RED http in
```

## Install (Ubuntu/Debian nginx)

```bash
# 1. token -> device id map
sudo install -d -m 700 /etc/nginx/gridtoken
TOKEN=$(openssl rand -hex 32)
echo "\"Bearer $TOKEN\" gridtoken-xxxxxx;" | sudo tee /etc/nginx/gridtoken/tokens.map >/dev/null
sudo chmod 600 /etc/nginx/gridtoken/tokens.map
sudo cp deploy/nginx/gridtoken-map.conf /etc/nginx/conf.d/

# 2. upstream auth (prompts locally for Node-RED httpNodeAuth, verifies it first)
sudo install -m 700 deploy/nginx/gridtoken-set-upstream-auth /usr/local/sbin/
echo 'proxy_set_header Authorization "";' | sudo tee /etc/nginx/gridtoken/upstream-auth.conf >/dev/null
sudo /usr/local/sbin/gridtoken-set-upstream-auth     # skip if Node-RED has no httpNodeAuth

# 3. paste deploy/nginx/gridtoken-location.conf into your Node-RED server block, then:
sudo nginx -t && sudo systemctl reload nginx
```

Put the token in the device's `.env` as `INGEST_TOKEN`, then flash or OTA the device.

- The device id is whatever you put in `tokens.map` (use the device's
  `gridtoken-xxxxxx` name). Add a `manual` token too if you want to post hand-read
  meter values to `/gridtoken/reading`.
- The default `map_hash_bucket_size` (64) is too small for these keys; the
  shipped `gridtoken-map.conf` sets 128.
- Always run `nginx -t` and reload only if it passes. `nginx -t | tail && reload`
  ignores the test result because of the pipe.

## Endpoints

| Path | Body | Result |
|---|---|---|
| `POST /gridtoken/ingest` | JPEG (`image/jpeg`) | frame stored + `gridtoken_ingest` point |
| `POST /gridtoken/reading` | `{"kwh": 59.23, "confidence": 0.98}` | `pln_prepaid` point (`source` = `manual` for the `manual` token, else `camera`) |

## Grafana

`deploy/grafana-available-credit.flux` computes the remaining credit in Rupiah:
the last `pln_prepaid.kwh` reading minus the grid energy drawn since then (the
integral of a grid power series), times the tariff. Adjust the measurement names
and price to your setup.
