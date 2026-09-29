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
1. **Darkness map:** `1 − pixel / local background` (Gaussian blur, 40 px), so lit
   segments score high and unlit "ghost" segments score near 0, whatever the exposure.
2. **Alignment:** the LCD window's dark borders (right edge and top edge) are
   located and the calibrated boxes are shifted, so a slightly bumped camera still reads.
3. **Segments:** each segment is sampled as a short strip along its direction,
   keeping the darkest line within ±`across` px. Italic digits are handled with `slant`.
4. **Decode:** lit patterns map to digits, and leading blanks are allowed. The value
   has a fixed number of decimals (SMI-810: 2). The screen code must match
   `balance_screen_code` (SMI-810: `37`), or the frame is ignored.
5. **Confidence** = the smallest distance of any segment from the threshold,
   normalised by `confidence_spread`. Below `min_confidence` the result is `ok: false`.

## Calibrating a new installation

Take a backlit frame, run with `--debug`, and adjust `profiles/<device>.json`:
- `main.x0`: the **bottom-left** x of the rightmost digit. Also set `y0` (top), `w`, `h`, `pitch` and `slant`.
- `code`: the same for the small screen-code digits.
- `anchors`: the LCD window's right border x and top border y in that frame.
- Check that every lit segment's circle is red and every unlit one is green in the overlay.
  `segment_thr` sits halfway between the weakest lit and the strongest unlit value.

Plausibility (the balance only falls, at most at the supply limit, and rises only on
confirmed top-ups) lives in the Node-RED flow, not here.
