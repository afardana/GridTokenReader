#!/usr/bin/env python3
"""Run the recogniser over a folder of frames and summarise the results.

    evaluate.py FRAMES_DIR --profile profiles/<device>.json [--debug-dir out/]

Prints one line per frame (time order) and a summary: readable %, distinct
values, confidence distribution, and frames whose value is out of step with its
neighbours (candidates to inspect with --debug-dir).
"""
import argparse
import json
import pathlib
import statistics

import gridtoken_recognise as R


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("frames")
    ap.add_argument("--profile", required=True)
    ap.add_argument("--debug-dir")
    a = ap.parse_args()
    profile = json.load(open(a.profile))
    files = sorted(p for p in pathlib.Path(a.frames).rglob("*.jpg") if p.name != "latest.jpg")
    rows = []
    for f in files:
        dbg = str(pathlib.Path(a.debug_dir) / (f.parent.name + "_" + f.stem + ".png")) if a.debug_dir else None
        r = R.recognise(str(f), profile, dbg)
        rows.append((f, r))
        print(f"{f.parent.name}/{f.name}  {r['kwh'] if r['ok'] else '-':>8}  conf {r['confidence']:.2f}  "
              f"digits '{r.get('digits', '')}' code '{r.get('code_digits', '')}'  {r['reason'] or ''}")
    ok = [r for _, r in rows if r["ok"]]
    print(f"\n{len(ok)}/{len(rows)} readable ({100 * len(ok) / max(1, len(rows)):.1f}%)")
    if ok:
        confs = [r["confidence"] for r in ok]
        print(f"confidence: min {min(confs):.2f}  median {statistics.median(confs):.2f}")
        print("distinct values:", sorted({r["kwh"] for r in ok}, reverse=True)[:20])
        vals = [(f, r["kwh"]) for f, r in rows if r["ok"]]
        odd = [f.name for (_, p), (f, v), (_, n) in zip(vals, vals[1:], vals[2:]) if v != p and v != n and p == n]
        print("isolated outliers (value differs from both neighbours, which agree):", odd or "none")


if __name__ == "__main__":
    main()
