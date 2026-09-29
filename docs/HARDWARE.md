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
With a bare USB-UART: tie **IO0 → GND**, reset, flash, then remove the jumper and
reset again.

## Power

- Use a solid **5 V ≥ 1 A** supply on the 5V pin. The flash LED plus a Wi-Fi TX burst
  can pull >400 mA peaks, and weak supplies cause brownout resets
  (`Brownout detector was triggered`).
- Keep the USB/power lead short and thick. Add a 470–1000 µF capacitor across
  5V/GND near the module if the brownouts persist.

## Optics & mounting tips

- The stock lens focuses at ~1 m. For a meter LCD at 8–12 cm, carefully rotate the
  lens (break the glue dab first) until the digits are sharp in `/capture`.
- Mount the camera square to the LCD and slightly off-axis if the flash reflects
  in the glass.
- Enclose camera + LCD in a light-tight hood for consistent exposure, day and night.

## Pins used

| GPIO | Use |
|---|---|
| 4 | Flash LED (white) |
| 33 | Status LED (red, active low) |
| 0, 5, 18–27, 32, 34–36, 39 | Camera bus |
