# Hardware

## Verified display target

ESP32-D0WD-V3 / ESP32-32E, 4 MB flash, no PSRAM; 2.8-inch 240×320 ILI9341 rotated to **320×240**; XPT2046 resistive touch. The tested board has an E32R28T-style pinout. This is not a claim that every board sold under the same marketing name is identical.

| Function | GPIO |
| --- | --- |
| LCD SCLK / MOSI / MISO | 14 / 13 / 12 |
| LCD DC / CS / reset | 2 / 15 / EN (software `-1`) |
| LCD backlight | 21 |
| Touch SCLK / MOSI / MISO | 25 / 32 / 39 |
| Touch CS / IRQ | 33 / 36 |
| BOOT recovery button | 0 (active low) |
| RGB LEDs, off at boot | 22 / 16 / 17 (active low) |

LCD SPI uses SPI2 at 40 MHz. Touch uses an independent SPI3 bus at 1 MHz. The framebuffer is 8-bit color to fit an ESP32 without PSRAM. UI values update at configurable 2/3/5/10-second intervals; touch events continue while the network task waits.

GPIO21 is used for PWM brightness. Do not copy these pins to an unrelated display board. No SD card or battery is required. ESP32-S3, capacitive touch and alternative panels are not validated targets in v0.1.0.

The pin mapping is currently in the `Panel` constructor and backlight/BOOT setup. A new board needs a separately documented configuration and a physical test, including recovery-button behavior, touch calibration and framebuffer allocation. It should not silently replace the verified default.
