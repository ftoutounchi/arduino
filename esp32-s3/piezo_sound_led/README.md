# ESP32-S3 Piezo Sound LED

This PlatformIO project samples a piezo sensor on an ESP32-S3 analog input and turns an LED on when the estimated sound level reaches about 70 dB.

Important: a bare piezo disc does not measure true dB SPL by itself. The code prints the sensor RMS value so you can calibrate the value that corresponds to your target level.

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

## Calibration

1. Upload the project.
2. Open the serial monitor.
3. Play or produce a known 70 dB sound at the sensor position.
4. Watch the printed `rms=` value.
5. Replace `RMS_AT_70_DB` in `src/main.cpp` with that value.

The LED uses hysteresis:

- On at `10 dB`
- Off below `7 dB`

This prevents rapid flickering around the threshold.

## Commands

```sh
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor -b 115200
```
