#!/usr/bin/env python3
"""GridTokenReader 7-segment recogniser (Phase 2, backend).

Reads the remaining-kWh value and the screen code off a prepaid-meter LCD frame
by sampling each segment of each digit position, using a per-installation
calibration (profiles/<device>.json). No ML: the LCD's digit positions are
fixed, so the digits can be sampled directly. Unlit "ghost" segments are
separated from lit ones by darkness relative to the local background.

    gridtoken_recognise.py FRAME.jpg --profile profiles/gridtoken-9b16b8.json [--debug out.png]

Prints one JSON object: {"ok": bool, "kwh": float|null, "code": str|null, ...}.
Needs Pillow + numpy.
"""

import argparse
import json
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# Segment sample points in unit digit coordinates (u: 0 left..1 right, v: 0 top..1 bottom),
# before slant. Standard naming: a top, b upper-right, c lower-right, d bottom,
# e lower-left, f upper-left, g middle.
SEGMENTS = {
    "a": (0.50, 0.05), "b": (0.92, 0.27), "c": (0.92, 0.73), "d": (0.50, 0.95),
    "e": (0.08, 0.73), "f": (0.08, 0.27), "g": (0.50, 0.50),
}
PATTERNS = {
    "abcdef": "0", "bc": "1", "abdeg": "2", "abcdg": "3", "bcfg": "4",
    "acdfg": "5", "acdefg": "6", "abc": "7", "abcf": "7", "abcdefg": "8", "abcdfg": "9",
    "": " ",
}


def darkness_map(img):
    """0 = like the local background, 1 = much darker (a lit LCD segment)."""
    gray = img.convert("L")
    bg = gray.filter(ImageFilter.GaussianBlur(40))
    g = np.asarray(gray, dtype=np.float32)
    b = np.asarray(bg, dtype=np.float32) + 1.0
    return np.clip(1.0 - g / b, 0.0, 1.0)


def first_run(profile, thr, reverse=False):
    idx = range(len(profile) - 1, -1, -1) if reverse else range(len(profile))
    for i in idx:
        if profile[i] > thr:
            return i
    return None


def find_offset(dark, cal):
    """Shift (dx, dy) of the LCD window vs. calibration, from its dark borders."""
    a = cal["anchors"]
    dx = dy = 0
    # right border: first strongly dark column scanning right from inside the LCD
    y0, y1 = a["band_y"]
    cols = dark[y0:y1, :].mean(axis=0)
    x_in = a["right_border_x"] - a["search"]
    r = first_run(cols[x_in:x_in + 2 * a["search"]], a["border_thr"])
    if r is not None:
        dx = (x_in + r) - a["right_border_x"]
    # top border: last dark row scanning up from inside the LCD
    x0, x1 = a["band_x"]
    rows = dark[:, x0:x1].mean(axis=1)
    y_in = a["top_border_y"] + a["search"]
    t = first_run(rows[y_in - 2 * a["search"]:y_in], a["border_thr"], reverse=True)
    if t is not None:
        dy = (y_in - 2 * a["search"] + t) - a["top_border_y"]
    return int(dx), int(dy)


def seg_point(box, u, v, slant):
    """box = (x0 bottom-left, y0 top, w, h); italic digits lean right by slant*h at the top."""
    x0, y0, w, h = box
    return x0 + u * w + (1.0 - v) * slant * h, y0 + v * h


HORIZONTAL = set("adg")


def sample(dark, x, y, along, across, horizontal):
    """Darkness of the stroke near (x, y): average along the segment, then take the
    darkest line within +-across, so small misalignments don't matter."""
    h, w = dark.shape
    rx, ry = (along, across) if horizontal else (across, along)
    xa, xb = int(max(0, x - rx)), int(min(w, x + rx + 1))
    ya, yb = int(max(0, y - ry)), int(min(h, y + ry + 1))
    if xb - xa < rx or yb - ya < ry:
        return None  # (mostly) outside the frame
    win = dark[ya:yb, xa:xb]
    lines = win.mean(axis=1) if horizontal else win.mean(axis=0)
    return float(lines.max())


def read_group(dark, group, dx, dy):
    """Sample every segment of every digit position in a group (main value / code)."""
    out = []
    for i in range(group["count"]):
        box = (group["x0"] + dx - i * group["pitch"], group["y0"] + dy, group["w"], group["h"])
        segs = {}
        for name, (u, v) in SEGMENTS.items():
            x, y = seg_point(box, u, v, group["slant"])
            d = sample(dark, x, y, group["along"], group["across"], name in HORIZONTAL)
            segs[name] = {"x": x, "y": y, "d": d}
        out.append({"box": box, "segs": segs})
    return out[::-1]  # left-to-right


def classify(positions, thr):
    digits, margins = [], []
    for p in positions:
        vals = [s["d"] for s in p["segs"].values()]
        if any(v is None for v in vals):
            p["char"] = None  # outside the frame
            digits.append(None)
            continue
        lit = "".join(n for n in "abcdefg" if p["segs"][n]["d"] >= thr)
        p["lit"] = lit
        p["char"] = PATTERNS.get(lit, "?")
        digits.append(p["char"])
        margins.extend(abs(v - thr) for v in vals)
    return digits, margins


def recognise(path, profile, debug=None):
    cal = profile["calibration"]
    img = Image.open(path).convert("RGB")
    dark = darkness_map(img)
    dx, dy = find_offset(dark, cal)

    main = read_group(dark, cal["main"], dx, dy)
    code = read_group(dark, cal["code"], dx, dy) if "code" in cal else []
    thr = cal["segment_thr"]
    main_digits, m1 = classify(main, thr)
    code_digits, m2 = classify(code, thr)
    margins = m1 + m2
    # confidence: how far the least clear-cut segment is from the threshold (0..1)
    spread = cal.get("confidence_spread", 0.15)
    confidence = round(min(1.0, min(margins) / spread), 3) if margins else 0.0

    result = {"ok": False, "kwh": None, "code": None, "confidence": confidence,
              "digits": "".join(c if c else "_" for c in main_digits),
              "code_digits": "".join(c if c else "_" for c in code_digits),
              "offset": [dx, dy], "reason": None}

    visible = [c for c in main_digits if c is not None]
    text = "".join(visible).lstrip(" ")
    if "?" in text or not text or " " in text:
        result["reason"] = "unreadable digits"
    elif len(text) < cal["main"]["decimals"] + 1:
        result["reason"] = "too few digits"
    else:
        result["kwh"] = int(text) / (10 ** cal["main"]["decimals"])
    ctext = "".join(c for c in code_digits if c is not None).strip()
    if code:
        result["code"] = ctext if ctext and "?" not in ctext and " " not in ctext else None
        want = profile.get("balance_screen_code")
        if result["kwh"] is not None and want and result["code"] != want:
            result["reason"] = f"screen code {result['code']!r} is not the balance screen ({want})"
            result["kwh"] = None
    if result["kwh"] is not None and confidence < cal.get("min_confidence", 0.2):
        result["reason"] = "low confidence"
        result["kwh"] = None
    result["ok"] = result["kwh"] is not None

    if debug:
        draw_debug(img, dark, main + code, thr, dx, dy, result, debug)
    return result


def draw_debug(img, dark, positions, thr, dx, dy, result, out):
    vis = img.copy()
    d = ImageDraw.Draw(vis)
    for p in positions:
        x0, y0, w, h = p["box"]
        d.rectangle([x0, y0, x0 + w, y0 + h], outline=(255, 255, 0))
        for name, s in p["segs"].items():
            if s["d"] is None:
                continue
            on = s["d"] >= thr
            col = (255, 40, 40) if on else (40, 255, 40)
            d.ellipse([s["x"] - 5, s["y"] - 5, s["x"] + 5, s["y"] + 5], outline=col, width=2)
            d.text((s["x"] + 6, s["y"] - 6), f"{name}{s['d']:.2f}", fill=col)
        if p.get("char") is not None:
            d.text((x0 + w / 2 - 4, y0 + h + 4), repr(p["char"]), fill=(255, 255, 255))
    d.text((8, 8), f"{result} off=({dx},{dy})", fill=(255, 255, 255))
    vis.save(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("frame")
    ap.add_argument("--profile", required=True)
    ap.add_argument("--debug", help="write an annotated PNG here")
    args = ap.parse_args()
    with open(args.profile) as f:
        profile = json.load(f)
    try:
        res = recognise(args.frame, profile, args.debug)
    except Exception as e:  # never crash the caller (Node-RED exec)
        res = {"ok": False, "kwh": None, "reason": f"error: {e}"}
    print(json.dumps(res))
    return 0


if __name__ == "__main__":
    sys.exit(main())
