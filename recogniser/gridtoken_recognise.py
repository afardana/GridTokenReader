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


def _blur(arr, radius):
    return np.asarray(Image.fromarray(arr.astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32)


def darkness_map(img, radius=40, lcd=None):
    """0 = like the local background, 1 = much darker (a lit LCD segment).

    With `lcd` (top, bottom, right) the background is estimated from pixels inside
    the LCD window only, so the dark bezel next to the outer digits does not drag
    it down. Without it, a plain blur: more forgiving when part of the window is
    shadowed (e.g. by the bezel at an oblique viewing angle).
    """
    g = np.asarray(img.convert("L"), dtype=np.float32)
    if lcd is None:
        return np.clip(1.0 - g / (_blur(g, radius) + 1.0), 0.0, 1.0)
    top, bottom, right = lcd
    mask = np.zeros_like(g)
    mask[top + 4:bottom - 3, :max(0, right - 4)] = 1.0
    bg = _blur(g * mask, radius) / np.maximum(_blur(mask * 255.0, radius) / 255.0, 0.02)
    return np.clip(1.0 - g / (bg + 1.0), 0.0, 1.0) * mask


def _smooth(v, k=9):
    return np.convolve(v, np.ones(k) / k, mode="same")


def find_lcd(img):
    """Locate the bright LCD window by its steepest brightness edges.

    Returns (top, bottom, right) in pixels. The left edge is not used: at close
    range it is outside the frame.
    """
    g = np.asarray(img.convert("L").filter(ImageFilter.GaussianBlur(4)), dtype=np.float32)
    h, w = g.shape
    m = 12  # ignore smoothing artefacts at the array ends
    rows = np.gradient(_smooth(g[:, w // 5:4 * w // 5].mean(axis=1)))
    top = int(m + np.argmax(rows[m:h // 2]))
    bottom = int(h // 3 + np.argmin(rows[h // 3:h - m]))
    if bottom - top < 40:
        return None
    cols = np.gradient(_smooth(g[top + 10:bottom - 10, :].mean(axis=0)))
    right = int(w // 2 + np.argmin(cols[w // 2:w - m]))
    return top, bottom, right


def make_transform(lcd, ref, dx=0):
    """Map calibration-frame pixels to this frame: same LCD window, shifted and scaled.
    `dx` nudges the result horizontally (see the search in recognise())."""
    top, bottom, right = lcd
    scale = (bottom - top) / float(ref["bottom"] - ref["top"])
    return scale, (lambda x: right + dx + (x - ref["right"]) * scale), (lambda y: top + (y - ref["top"]) * scale)


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
    xa, xb = int(max(0, round(x - rx))), int(min(w, round(x + rx) + 1))
    ya, yb = int(max(0, round(y - ry))), int(min(h, round(y + ry) + 1))
    if xb - xa < rx or yb - ya < ry:
        return None  # (mostly) outside the frame
    win = dark[ya:yb, xa:xb]
    lines = win.mean(axis=1) if horizontal else win.mean(axis=0)
    return float(lines.max())


def read_group(dark, group, tf):
    """Sample every segment of every digit position in a group (main value / code)."""
    scale, tx, ty = tf
    out = []
    for i in range(group["count"]):
        box = (tx(group["x0"] - i * group["pitch"]), ty(group["y0"]), group["w"] * scale, group["h"] * scale)
        segs = {}
        for name, (u, v) in SEGMENTS.items():
            x, y = seg_point(box, u, v, group["slant"])
            d = sample(dark, x, y, group["along"] * scale, group["across"] * scale, name in HORIZONTAL)
            segs[name] = {"x": x, "y": y, "d": d}
        out.append({"box": box, "segs": segs})
    return out[::-1]  # left-to-right


def split_threshold(values, fallback, low_max=0.15, min_gap=0.03):
    """Per-frame lit/unlit threshold. Unlit and ghost segments form a tight cluster
    near 0 whatever the focus; lit ones are spread out above it (glare can weaken
    a lit stroke a lot). So take the first gap of at least `min_gap` above the
    unlit cluster, not the widest gap. Returns (threshold, gap)."""
    v = sorted(values)
    for lo, hi in zip(v, v[1:]):
        if lo < low_max and hi - lo >= min_gap:
            return (lo + hi) / 2.0, hi - lo
    return fallback, 0.0


def classify(positions, thr):
    digits = []
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
    return digits


# Horizontal nudges tried around the detected position, nearest first. The right
# LCD border is a weak anchor when it sits near the frame edge.
DX_SEARCH = [0] + [d * sign for d in range(4, 29, 4) for sign in (1, -1)]


def _read(img, profile, lcd, ref, scale, masked):
    """Best reading over the horizontal search, with one background method."""
    dark = darkness_map(img, (25 if masked else 40) * scale, lcd if masked else None)
    best = None
    for dx in DX_SEARCH:
        res, positions, thr = _read_at(dark, profile, make_transform(lcd, ref, dx), masked)
        res["dx"] = dx
        # prefer readable results, then fewer undecodable digits, then the widest gap
        key = (res["ok"], -(res["digits"] + res["code_digits"]).count("?"), res["confidence"], res["gap"])
        if best is None or key > best[0]:
            best = (key, res, positions, thr)
        if res["ok"] and res["confidence"] >= 1.0:
            break  # clean read at the nearest offset: stop searching
    return best[1], best[2], best[3]


def _read_at(dark, profile, tf, masked):
    """One reading attempt at one alignment."""
    cal = profile["calibration"]
    main = read_group(dark, cal["main"], tf)
    code = read_group(dark, cal["code"], tf) if "code" in cal else []
    values = [sg["d"] for p in main + code for sg in p["segs"].values() if sg["d"] is not None]
    thr, gap = split_threshold(values, cal["segment_thr"])
    main_digits = classify(main, thr)
    code_digits = classify(code, thr)
    # confidence: width of the gap between the unlit and lit groups (0..1)
    confidence = round(min(1.0, gap / cal.get("confidence_gap", 0.06)), 3)

    res = {"ok": False, "kwh": None, "code": None, "confidence": confidence,
           "digits": "".join(c if c else "_" for c in main_digits),
           "code_digits": "".join(c if c else "_" for c in code_digits),
           "method": "masked" if masked else "blur", "thr": round(thr, 3), "gap": round(gap, 4), "reason": None}
    text = "".join(c for c in main_digits if c is not None).lstrip(" ")
    if "?" in text or not text or " " in text:
        res["reason"] = "unreadable digits"
    elif len(text) < cal["main"]["decimals"] + 1:
        res["reason"] = "too few digits"
    else:
        res["kwh"] = int(text) / (10 ** cal["main"]["decimals"])
    if code:
        ctext = "".join(c for c in code_digits if c is not None).strip()
        res["code"] = ctext if ctext and "?" not in ctext and " " not in ctext else None
        want = profile.get("balance_screen_code")
        if res["kwh"] is not None and want and res["code"] != want:
            res["reason"] = f"screen code {res['code']!r} is not the balance screen ({want})"
            res["kwh"] = None
    if res["kwh"] is not None and confidence < cal.get("min_confidence", 0.2):
        res["reason"] = "low confidence"
        res["kwh"] = None
    res["ok"] = res["kwh"] is not None
    return res, main + code, thr


def recognise(path, profile, debug=None):
    cal = profile["calibration"]
    img = Image.open(path).convert("RGB")
    lcd = find_lcd(img)
    ref = cal["lcd"]
    scale = (lcd[1] - lcd[0]) / float(ref["bottom"] - ref["top"]) if lcd else 0
    if not lcd or not 0.7 <= scale <= 1.4:
        return {"ok": False, "kwh": None, "code": None, "confidence": 0.0, "lcd": lcd,
                "reason": "LCD window not found (glare, dark frame, or camera moved too far)"}
    # Two background estimates, cross-checked (see darkness_map).
    a, pos_a, thr_a = _read(img, profile, lcd, ref, scale, masked=True)
    b, pos_b, thr_b = _read(img, profile, lcd, ref, scale, masked=False)
    thr = thr_a
    if a["ok"] and b["ok"] and a["kwh"] != b["kwh"]:
        result, positions = dict(a), pos_a
        result.update(ok=False, kwh=None, reason=f"methods disagree ({a['kwh']} vs {b['kwh']})")
    elif a["ok"] or not b["ok"]:
        result, positions = a, pos_a
        if a["ok"] and b["ok"]:
            result["confidence"] = max(a["confidence"], b["confidence"])
    else:
        result, positions, thr = b, pos_b, thr_b
    result.update(lcd=list(lcd), scale=round(scale, 3))

    if debug:
        draw_debug(img, positions, thr, lcd, result, debug)
    return result


def draw_debug(img, positions, thr, lcd, result, out):
    vis = img.copy()
    d = ImageDraw.Draw(vis)
    top, bottom, right = lcd
    d.line([(0, top), (right, top), (right, bottom), (0, bottom)], fill=(255, 0, 255), width=2)
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
    d.text((8, 8), str(result), fill=(255, 255, 255))
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
