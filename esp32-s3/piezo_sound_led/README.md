# ESP32-S3 Piezo Sound LED

This PlatformIO project samples a piezo sensor on an ESP32-S3 analog input and turns the onboard LED on when sound or vibration is detected.

Important: a bare piezo disc does not measure true dB SPL by itself. This test firmware detects raw movement from the piezo instead of trying to calculate real dB.

## Default Pins

- Piezo analog signal: `GPIO1`
- Onboard RGB LED: `GPIO48`
- USB serial monitor: `115200`

Change pins in `platformio.ini`:

```ini
build_flags =
  -DPIEZO_PIN=1
  -DLED_PIN=48
  -DUSE_NEOPIXEL_LED=1
```

For a normal external LED instead:

```ini
build_flags =
  -DPIEZO_PIN=1
  -DLED_PIN=2
  -DUSE_NEOPIXEL_LED=0
```

## Wiring Notes

- Do not connect a raw piezo directly if it can create voltage spikes above 3.3 V.
- Use a resistor divider or bias circuit so the ADC pin sits around the middle of the 0-3.3 V range.
- Add protection such as a series resistor and clamp diodes if the piezo can be hit or bent hard.
- Connect all grounds together.

## Sensitivity

Open the serial monitor and watch the printed `p2p=` and `rms=` values.

- If the LED never turns on, lower `SOUND_P2P_THRESHOLD` or `SOUND_RMS_THRESHOLD` in `src/main.cpp`.
- If the LED stays on all the time, raise those values.

Default detection thresholds are `p2p >= 30` or `rms >= 8.0`.

## Commands

```sh
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor -b 115200
```
