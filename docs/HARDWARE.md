# Hardware

## Reference board

| Item | Value |
|---|---|
| Module | AI-Thinker ESP32-CAM |
| SoC | ESP32-D0WD rev 1.0, dual core 240 MHz, Wi-Fi + BT |
| Flash | 4 MB (GigaDevice) |
| PSRAM | 4 MB (required for SVGA+ frames and, later, on-device inference) |
| Camera | OV2640 (2 MP, fixed focus) |
| Programmer | ESP32-CAM-MB (CH340, USB `1a86:7523`), with auto-reset/boot |

Verify your own board:

```bash
esptool --port /dev/cu.usbserial-XXXX chip-id
esptool --port /dev/cu.usbserial-XXXX flash-id
```

## Flashing

With the ESP32-CAM-MB, just `pio run -t upload`. It toggles IO0/EN automatically.
The CH340 on these boards corrupts data at ≥460800 baud, so `upload_speed` is
230400. After the first USB flash, use OTA (`pio run -e esp32cam-ota -t upload`
or `http://<device>/update`).
With a bare USB-UART: tie **IO0 → GND**, reset, flash, then remove the jumper and
reset again.

## Power

- Use a solid **5 V ≥ 1 A** supply on the 5V pin. The flash LED plus a Wi-Fi TX burst
  can pull >400 mA peaks, and weak supplies cause brownout resets
  (`Brownout detector was triggered`).
- Keep the USB/power lead short and thick. Add a 470–1000 µF capacitor across
  5V/GND near the module if the brownouts persist.
- The firmware starts Wi-Fi 1.5 s before the camera and caps TX power at 15 dBm to
  soften the peaks. Powered from a laptop USB port via the ESP32-CAM-MB, the
  reference unit still browns out once on the very first boot after a USB flash
  (full RF calibration), then runs normally. A proper supply fixes this.

## microSD card

- Runs in **1-bit SD mode** (CLK 14, CMD 15, D0 2). 4-bit mode would use GPIO 4 as
  DAT1, and GPIO 4 drives the flash LED.
- Format as **FAT32**. Cards >32 GB ship as exFAT and must be reformatted.
- Captures go to `/captures/YYYY-MM-DD/YYYYMMDDTHHMMSSZ.jpg` (UTC). Before each
  save, the oldest files are deleted until ≥10% of the card is free.
- On mount, the firmware writes a probe file and checks that it appears in a real
  directory listing. Worn-out or counterfeit cards often acknowledge writes and
  then silently discard them. Such a card is refused, and `/sd` reports why.
- Browse over HTTP: `/sd` (status), `/sd/list`, `/sd/list?day=YYYY-MM-DD`,
  `/sd/file?path=/captures/...`.

## Lighting (dark / sealed compartments)

The white LED on GPIO 4 lights the LCD for every capture. It is PWM-driven on
LEDC channel 7 (the camera clock owns channel 0) and is on only during a capture.

| Setting | Default | Notes |
|---|---|---|
| `led` | 128 / 255 | Full power at ~10 cm usually blows out an LCD. Go as low as keeps the digits crisp. |
| `settle_ms` | 600 | LED-on time before the shot so auto-exposure/white balance converge from darkness. |
| `exposure_mode` | auto | Switch to `manual` once tuned. The scene never changes in a sealed box, so fixed exposure/gain gives identical frames. |
| `exposure` / `gain` | 300 / 0 | Used in manual mode (0–1200 / 0–30). |

Tune from the web UI (`http://<device>/`) or directly, e.g.
`/settings?led=90&settle=400&aec=manual&exposure=350&gain=2`. Values persist in
NVS across reboots. Glare on the LCD window: diffuse the LED (translucent tape) or
tilt the camera a few degrees.

## Optics & mounting tips

- The stock lens focuses at ~1 m. For a meter LCD at 8–12 cm, carefully rotate the
  lens (break the glue dab first) until the digits are sharp in `/capture`.
- Mount the camera square to the LCD and slightly off-axis if the flash reflects
  in the glass.
- Enclose camera + LCD in a light-tight hood for consistent exposure, day and night.

## Pins used

| GPIO | Use |
|---|---|
| 4 | Flash LED (white), PWM via LEDC ch 7 |
| 33 | Status LED (red, active low) |
| 0, 5, 18–27, 32, 34–36, 39 | Camera bus |
| 2, 14, 15 | microSD (1-bit: D0, CLK, CMD) |
