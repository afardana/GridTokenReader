"""PlatformIO pre-script: turn the repo-root `.env` into build configuration.

* Writes `<build_dir>/generated/env_secrets.h` with one #define per known key,
  so secrets never appear on compiler command lines or in git.
* For OTA environments (upload_protocol = espota) sets the upload host from
  OTA_HOST and passes OTA_PASSWORD to espota.py right before uploading.

Real environment variables with the same names override `.env` (handy for CI).
"""

import json
import os
from pathlib import Path

Import("env")  # noqa: F821  (provided by SCons)

ENV_FILE = Path(env.subst("$PROJECT_DIR")).parent / ".env"  # noqa: F821

# key -> "str" | "int"; only these reach the firmware.
KEYS = {
    "WIFI_SSID": "str",
    "WIFI_PASSWORD": "str",
    "MQTT_HOST": "str",
    "MQTT_PORT": "int",
    "MQTT_USER": "str",
    "MQTT_PASSWORD": "str",
    "INGEST_URL": "str",
    "INGEST_CONNECT_IP": "str",
    "INGEST_TOKEN": "str",
    "INGEST_USER": "str",
    "INGEST_PASSWORD": "str",
    "OTA_PASSWORD": "str",
    "CAPTURE_INTERVAL_S": "int",
}
ALIASES = {"SSID": "WIFI_SSID", "PASS": "WIFI_PASSWORD", "PASSWORD": "WIFI_PASSWORD"}


def parse_env_file(path):
    values = {}
    if not path.exists():
        return values
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, val = line.partition("=")
        key = key.strip().removeprefix("export ").strip()
        val = val.strip()
        if len(val) >= 2 and val[0] == val[-1] and val[0] in "\"'":
            val = val[1:-1]
        elif " #" in val:  # dotenv-style inline comment on unquoted values
            val = val.split(" #", 1)[0].rstrip()
        values[ALIASES.get(key, key)] = val
    return values


values = parse_env_file(ENV_FILE)
for key in list(KEYS) + ["OTA_HOST"]:
    if os.environ.get(key):
        values[key] = os.environ[key]

lines = ["#pragma once", "// Generated from .env by scripts/load_env.py - do not edit or commit."]
for key, kind in KEYS.items():
    if key not in values or values[key] == "":
        continue
    val = values[key]
    if kind == "int":
        lines.append(f"#define {key} {int(val)}")
    else:
        lines.append(f"#define {key} {json.dumps(val)}")  # JSON escaping is valid C

gen_dir = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
gen_dir.mkdir(parents=True, exist_ok=True)
header = gen_dir / "env_secrets.h"
content = "\n".join(lines) + "\n"
if not header.exists() or header.read_text() != content:
    header.write_text(content)
env.Append(CPPPATH=[str(gen_dir)])  # noqa: F821

found = [k for k in KEYS if values.get(k)]
print(f"[env] {ENV_FILE if ENV_FILE.exists() else 'no .env'}: {', '.join(found) or 'no keys'}")

# ---- OTA upload wiring -------------------------------------------------------
if env.GetProjectOption("upload_protocol", "") == "espota":  # noqa: F821
    if not env.subst("$UPLOAD_PORT") and values.get("OTA_HOST"):  # noqa: F821
        env.Replace(UPLOAD_PORT=values["OTA_HOST"])  # noqa: F821

    def _add_ota_auth(source, target, env):
        # espota's --debug echoes all options, including the password: drop it.
        env.Replace(UPLOADERFLAGS=[f for f in env["UPLOADERFLAGS"] if f != "--debug"])
        if values.get("OTA_PASSWORD"):
            env.Append(UPLOADERFLAGS=["--auth=" + values["OTA_PASSWORD"]])

    env.AddPreAction("upload", _add_ota_auth)  # noqa: F821
