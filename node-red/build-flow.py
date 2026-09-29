"""Builds the GridTokenReader Node-RED tab (HTTP ingest behind the nginx token gate).

Usage: python3 build-flow.py <influx_config_id> <org> <bucket> > flow.json

With influx_config_id "new", an InfluxDB v2 config node (http://127.0.0.1:8086)
is included; set its token in the Node-RED editor after importing.
"""
import json
import sys

INFLUX_CFG, ORG, BUCKET = sys.argv[1], sys.argv[2], sys.argv[3]
Z = "gtr0tab00000001"
FRAMES = "/var/lib/gridtoken/frames"


def fn(id_, name, code, x, y, wires, outputs=None):
    return {
        "id": id_, "type": "function", "z": Z, "name": name, "func": code,
        "outputs": outputs or len(wires), "timeout": 0, "noerr": 0,
        "initialize": "", "finalize": "", "libs": [], "x": x, "y": y, "wires": wires,
    }


VALIDATE_FRAME = r"""// Device identity comes from nginx (token -> X-Device-Id); never from the body.
const dev = String(msg.req.headers['x-device-id'] || '').replace(/[^a-z0-9-]/gi, '');
const buf = msg.payload;
const isJpeg = Buffer.isBuffer(buf) && buf.length > 1000 && buf[0] === 0xFF && buf[1] === 0xD8;
const resp = { req: msg.req, res: msg.res, statusCode: 204, payload: '' };
if (!dev || !isJpeg) {
    resp.statusCode = 400;
    resp.payload = 'expected image/jpeg from a known device';
    node.warn(`rejected frame from '${dev}' (${Buffer.isBuffer(buf) ? buf.length : typeof buf})`);
    return [null, resp, null];
}
let status = {};
try { status = JSON.parse(msg.req.headers['x-device-status'] || '{}'); } catch (e) { }
const frame = { device: dev, ts: Date.now(), image: buf, firmware: msg.req.headers['x-firmware'] || null, status };

const fields = { bytes: buf.length };
for (const k of ['rssi', 'heap', 'psram', 'uptime_s']) {
    if (Number.isFinite(status[k])) fields[k] = status[k];
}
const tags = { device: dev };
if (frame.firmware) tags.firmware = String(frame.firmware);
const metrics = { payload: [{ measurement: 'gridtoken_ingest', fields, tags }] };

node.status({ fill: 'green', shape: 'dot', text: `${dev} ${buf.length} B ${new Date().toLocaleTimeString()}` });
return [frame, resp, metrics];
"""

FRAME_PATHS = r"""// /var/lib/gridtoken/frames/<device>/<YYYY-MM-DD>/<HHMMSS>.jpg (+ latest.jpg)
const dir = env.get('GRIDTOKEN_FRAMES') || '""" + FRAMES + r"""';
const d = new Date(msg.ts);
const p2 = n => String(n).padStart(2, '0');
const day = `${d.getFullYear()}-${p2(d.getMonth() + 1)}-${p2(d.getDate())}`;
const t = `${p2(d.getHours())}${p2(d.getMinutes())}${p2(d.getSeconds())}`;
return [[
    { filename: `${dir}/${msg.device}/${day}/${t}.jpg`, payload: msg.image },
    { filename: `${dir}/${msg.device}/latest.jpg`, payload: msg.image },
]];
"""

RECOGNISE = r"""// Phase 2 hook (docs/ROADMAP.md in GridTokenReader): crop the LCD and read the
// digits here. Emit { device, kwh, confidence, source: 'camera' } to record a
// reading; return null while the recogniser is not implemented.
return null;
"""

VALIDATE_READING = r"""// POST /gridtoken/reading  {"kwh": 59.23, "confidence": 0.98}
// Used by the camera (Phase 3, on-device recognition) and for manual anchors
// (token 'manual'). Device identity comes from nginx.
const dev = String(msg.req.headers['x-device-id'] || '').replace(/[^a-z0-9-]/gi, '');
const p = (typeof msg.payload === 'object' && msg.payload) || {};
const kwh = Number(p.kwh);
const resp = { req: msg.req, res: msg.res, statusCode: 204, payload: '' };
if (!dev || !Number.isFinite(kwh) || kwh < 0 || kwh > 100000) {
    resp.statusCode = 400;
    resp.payload = 'expected JSON {"kwh": <number>}';
    return [null, resp];
}
const confidence = p.confidence == null ? 1 : Math.max(0, Math.min(1, Number(p.confidence) || 0));
return [{ device: dev, kwh, confidence, source: dev === 'manual' ? 'manual' : 'camera' }, resp];
"""

TO_INFLUX = r"""// pln_prepaid: remaining prepaid balance in kWh as read off the meter.
// Grafana can extrapolate between readings with a grid power integral (deploy/grafana-available-credit.flux).
flow.set('lastReading', { device: msg.device, kwh: msg.kwh, source: msg.source, ts: Date.now() });
node.status({ fill: 'blue', shape: 'dot', text: `${msg.kwh} kWh (${msg.source}) ${new Date().toLocaleString()}` });
return { payload: [{
    measurement: 'pln_prepaid',
    fields: { kwh: msg.kwh, confidence: msg.confidence },
    tags: { device: msg.device, source: msg.source },
}] };
"""

nodes = [
    {"id": Z, "type": "tab", "label": "GridTokenReader", "disabled": False,
     "info": "Prepaid meter camera (github.com/afardana/GridTokenReader).\n\n"
             "Endpoints sit behind the nginx token gate (docs/NGINX.md): a per-device bearer token is "
             "checked there (/etc/nginx/gridtoken/tokens.map) and mapped to X-Device-Id, and nginx "
             "adds httpNodeAuth. Frames: " + FRAMES + " (30-day retention). InfluxDB: "
             "gridtoken_ingest (device health), pln_prepaid (kWh readings).",
     "env": [{"name": "GRIDTOKEN_FRAMES", "value": FRAMES, "type": "str"}]},
    {"id": "gtr0cmt00000001", "type": "comment", "z": Z,
     "name": "Frames: POST /gridtoken/ingest (JPEG)  ·  Readings: POST /gridtoken/reading (JSON)",
     "info": "", "x": 330, "y": 40, "wires": []},

    {"id": "gtr0in000000001", "type": "http in", "z": Z, "name": "POST /gridtoken/ingest",
     "url": "/gridtoken/ingest", "method": "post", "upload": False, "swaggerDoc": "",
     "x": 170, "y": 120, "wires": [["gtr0fnvalframe1"]]},
    fn("gtr0fnvalframe1", "validate frame", VALIDATE_FRAME, 400, 120,
       [["gtr0fnpaths0001", "gtr0fnrecog0001"], ["gtr0resp0000001"], ["gtr0influx00001"]]),
    {"id": "gtr0resp0000001", "type": "http response", "z": Z, "name": "ack",
     "statusCode": "", "headers": {}, "x": 630, "y": 200, "wires": []},
    fn("gtr0fnpaths0001", "frame paths", FRAME_PATHS, 640, 80, [["gtr0file0000001"]]),
    {"id": "gtr0file0000001", "type": "file", "z": Z, "name": "save frame",
     "filename": "filename", "filenameType": "msg", "appendNewline": False,
     "createDir": True, "overwriteFile": "true", "encoding": "none",
     "x": 830, "y": 80, "wires": [[]]},
    fn("gtr0fnrecog0001", "recognise digits (Phase 2)", RECOGNISE, 680, 140, [["gtr0fntoinflux1"]]),

    {"id": "gtr0in000000002", "type": "http in", "z": Z, "name": "POST /gridtoken/reading",
     "url": "/gridtoken/reading", "method": "post", "upload": False, "swaggerDoc": "",
     "x": 170, "y": 280, "wires": [["gtr0fnvalread01"]]},
    fn("gtr0fnvalread01", "validate reading", VALIDATE_READING, 400, 280,
       [["gtr0fntoinflux1"], ["gtr0resp0000001"]]),
    fn("gtr0fntoinflux1", "pln_prepaid point", TO_INFLUX, 920, 200, [["gtr0influx00001"]]),
    {"id": "gtr0influx00001", "type": "influxdb batch", "z": Z, "influxdb": INFLUX_CFG,
     "precision": "", "retentionPolicy": "", "name": "InfluxDB (local)", "database": "",
     "precisionV18FluxV20": "ms", "retentionPolicyV18Flux": "", "org": ORG, "bucket": BUCKET,
     "x": 1150, "y": 200, "wires": []},

    {"id": "gtr0inject00001", "type": "inject", "z": Z, "name": "daily 03:30",
     "props": [{"p": "payload"}], "repeat": "", "crontab": "30 03 * * *", "once": False,
     "onceDelay": 0.1, "topic": "", "payload": "", "payloadType": "date",
     "x": 170, "y": 380, "wires": [["gtr0exec0000001"]]},
    {"id": "gtr0exec0000001", "type": "exec", "z": Z,
     "command": "find " + FRAMES + " -type f -name '*.jpg' ! -name latest.jpg -mtime +30 -delete; "
                "find " + FRAMES + " -mindepth 2 -type d -empty -delete",
     "addpay": "", "append": "", "useSpawn": "false", "timer": "120", "winHide": False,
     "oldrc": False, "name": "prune frames > 30 days", "x": 420, "y": 380,
     "wires": [[], [], []]},
]

if INFLUX_CFG == "new":
    for n in nodes:
        if n.get("influxdb") == "new":
            n["influxdb"] = "gtr0influxcfg01"
    nodes.append({"id": "gtr0influxcfg01", "type": "influxdb", "hostname": "127.0.0.1", "port": "8086",
                  "protocol": "http", "database": "", "name": "InfluxDB (set token)", "usetls": False,
                  "tls": "", "influxdbVersion": "2.0", "url": "http://127.0.0.1:8086", "timeout": "",
                  "rejectUnauthorized": True})

json.dump(nodes, sys.stdout, indent=4, ensure_ascii=False)
