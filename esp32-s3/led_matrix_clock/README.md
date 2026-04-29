# ESP32-S3 8x8 LED Matrix Clock

This project displays the current hour on an 8x8 RGB LED matrix using an ESP32-S3 board. Time is synchronized with NTP and updated every minute, with the display changing at the start of each hour.

## Wiring
- `LED_PIN` is set to GPIO 6 by default.
- Connect the LED matrix data input to GPIO 6.
- Connect the matrix power to 5V (or 3.3V if your board supports it) and ground to GND.

## Setup
1. Open `src/main.cpp`.
2. Set your Wi-Fi credentials if needed.
3. The timezone is set to Germany/Berlin with daylight saving.
4. Build and upload using PlatformIO.
