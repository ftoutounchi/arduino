#include <Arduino.h>
#include <math.h>

#ifndef PIEZO_PIN
#define PIEZO_PIN 1
#endif

#ifndef LED_PIN
#define LED_PIN 21
#endif

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif

#ifndef USE_NEOPIXEL_LED
#define USE_NEOPIXEL_LED 1
#endif

static constexpr uint16_t SOUND_P2P_THRESHOLD = 30;
static constexpr float SOUND_RMS_THRESHOLD = 8.0f;
static constexpr uint32_t SAMPLE_WINDOW_MS = 50;
static constexpr uint32_t SAMPLE_INTERVAL_US = 125; // about 8 kHz
static constexpr uint32_t LED_OFF_HOLD_MS = 250;
static constexpr uint32_t PRINT_INTERVAL_MS = 250;

struct SoundReading {
  float rmsCounts;
  uint16_t minRaw;
  uint16_t maxRaw;
  uint32_t samples;
};

bool ledOn = false;
uint32_t lastSoundMs = 0;
uint32_t lastPrintMs = 0;

void setLed(bool on) {
  ledOn = on;

#if USE_NEOPIXEL_LED
  // Waveshare ESP32-S3-Zero uses an addressable onboard RGB LED on GPIO21.
  neopixelWrite(LED_PIN, on ? 0 : 0, on ? 30 : 0, on ? 0 : 0);
#else
  digitalWrite(LED_PIN, (on == (LED_ACTIVE_HIGH != 0)) ? HIGH : LOW);
#endif
}

SoundReading readSoundWindow() {
  SoundReading reading;
  reading.rmsCounts = 0.0f;
  reading.minRaw = 4095;
  reading.maxRaw = 0;
  reading.samples = 0;

  float mean = 0.0f;
  float m2 = 0.0f;
  const uint32_t startUs = micros();
  uint32_t nextSampleUs = startUs;

  while ((micros() - startUs) < (SAMPLE_WINDOW_MS * 1000UL)) {
    const uint16_t raw = analogRead(PIEZO_PIN);

    reading.samples++;
    if (raw < reading.minRaw) {
      reading.minRaw = raw;
    }
    if (raw > reading.maxRaw) {
      reading.maxRaw = raw;
    }

    const float delta = static_cast<float>(raw) - mean;
    mean += delta / static_cast<float>(reading.samples);
    const float deltaAfterMean = static_cast<float>(raw) - mean;
    m2 += delta * deltaAfterMean;

    nextSampleUs += SAMPLE_INTERVAL_US;
    while (static_cast<int32_t>(micros() - nextSampleUs) < 0) {
      delayMicroseconds(5);
    }
  }

  if (reading.samples > 1) {
    reading.rmsCounts = sqrtf(m2 / static_cast<float>(reading.samples));
  }

  return reading;
}

uint16_t peakToPeak(const SoundReading &reading) {
  return reading.maxRaw - reading.minRaw;
}

bool soundDetected(const SoundReading &reading) {
  return peakToPeak(reading) >= SOUND_P2P_THRESHOLD ||
         reading.rmsCounts >= SOUND_RMS_THRESHOLD;
}

void printReading(const SoundReading &reading, bool detected) {
  Serial.print("raw min=");
  Serial.print(reading.minRaw);
  Serial.print(" max=");
  Serial.print(reading.maxRaw);
  Serial.print(" p2p=");
  Serial.print(peakToPeak(reading));
  Serial.print(" rms=");
  Serial.print(reading.rmsCounts, 1);
  Serial.print(" sound=");
  Serial.print(detected ? "yes" : "no");
  Serial.print(" led=");
  Serial.println(ledOn ? "on" : "off");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetPinAttenuation(PIEZO_PIN, ADC_11db);

#if !USE_NEOPIXEL_LED
  pinMode(LED_PIN, OUTPUT);
#endif
  setLed(true);

  Serial.println();
  Serial.println("ESP32-S3 piezo sound detector");
  Serial.println("LED stays on when quiet and turns off when sound is detected.");
}

void loop() {
  const SoundReading reading = readSoundWindow();
  const bool detected = soundDetected(reading);
  const uint32_t now = millis();

  if (detected) {
    lastSoundMs = now;
    setLed(false);
  } else if ((now - lastSoundMs) > LED_OFF_HOLD_MS) {
    setLed(true);
  }

  if ((now - lastPrintMs) >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printReading(reading, detected);
  }
}
