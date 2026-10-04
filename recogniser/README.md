# Recogniser (Phase 2)

`gridtoken_recognise.py` reads the balance (kWh) and the screen code off a frame by
sampling the 7 segments of each digit position. It needs Pillow and numpy only,
with no ML model. It is calibrated per installation in `profiles/<device>.json`.

```bash
python3 gridtoken_recognise.py frame.jpg --profile profiles/gridtoken-9b16b8.json --debug overlay.png
# {"ok": true, "kwh": 59.23, "code": "37", "confidence": 1.0, "digits": " 5923", ...}
python3 evaluate.py /var/lib/gridtoken/frames/gridtoken-9b16b8 --profile profiles/gridtoken-9b16b8.json
```

How it works:
1. **Alignment:** the bright LCD window is located by its steepest brightness edges
   (top, bottom, right). The calibrated boxes are then **shifted and scaled** to it,
   so a camera that was bumped or re-mounted a little closer still reads without
   re-calibration (window height within 0.7–1.4× of the calibration frame).
   The right border is a weak anchor when it sits near the frame edge, so the
   reader also tries small horizontal nudges (±28 px) and keeps the alignment that
   decodes cleanly with the widest lit/unlit gap.
2. **Darkness map:** `1 − pixel / local background`, so lit segments score high and
   unlit "ghost" segments score near 0, whatever the exposure. Two background
   estimates are used and cross-checked: one from pixels inside the LCD window only
   (the dark bezel can't hide the outer segments), and a plain blur (more forgiving
   when the bezel shadows part of the window). If both give a value and they
   differ, the frame is rejected.
3. **Segments:** each segment is sampled as a short strip along its direction,
   keeping the darkest line within ±`across` px. Italic digits are handled with `slant`.
4. **Decode:** lit patterns map to digits, and leading blanks are allowed. The value
   has a fixed number of decimals (SMI-810: 2). The screen code must match
   `balance_screen_code` (SMI-810: `37`), or the frame is ignored.
5. **Threshold and confidence:** the lit/unlit threshold is set per frame. Unlit and
   ghost segments form a tight cluster near 0; lit ones spread out above it (glare
   can weaken a lit stroke a lot). The threshold is the middle of the first gap of
   ≥0.03 above that cluster, so it doesn't depend on focus or exposure.
   Confidence = gap width / `confidence_gap`, capped at 1; below `min_confidence` the result is `ok: false`. `segment_thr` is only
   the fallback when a frame has no gap at all.

## Calibrating a new installation

Take a backlit frame, run with `--debug`, and adjust `profiles/<device>.json`:
- `main.x0`: the **bottom-left** x of the rightmost digit. Also set `y0` (top), `w`, `h`, `pitch` and `slant`.
- `code`: the same for the small screen-code digits.
- `lcd`: the LCD window edges (`top`, `bottom`, `right`) of that frame, as printed in the result's `lcd` field.
- Check that every lit segment's circle is red and every unlit one is green in the overlay.
  The result's `thr` field shows the threshold the frame chose.

Plausibility (the balance only falls, at most at the supply limit, and rises only on
confirmed top-ups) lives in the Node-RED flow, not here.
