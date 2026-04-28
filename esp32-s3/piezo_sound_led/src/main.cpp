#include <Arduino.h>
#include <math.h>

#ifndef PIEZO_PIN
#define PIEZO_PIN 1
#endif

#ifndef LED_PIN
#define LED_PIN 48
#endif

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif

#ifndef USE_NEOPIXEL_LED
#define USE_NEOPIXEL_LED 1
#endif

// Put a known ~70 dB sound near the piezo, read the printed RMS value,
// then replace this number so the dB estimate matches your hardware.
static constexpr float RMS_AT_70_DB = 120.0f;

static constexpr float LED_ON_DB = 10.0f;
static constexpr float LED_OFF_DB = 7.0f;
static constexpr uint32_t SAMPLE_WINDOW_MS = 50;
static constexpr uint32_t SAMPLE_INTERVAL_US = 125; // about 8 kHz
static constexpr uint32_t LED_HOLD_MS = 400;
static constexpr uint32_t PRINT_INTERVAL_MS = 250;

struct SoundReading {
  float rmsCounts;
  uint16_t minRaw;
  uint16_t maxRaw;
  uint32_t samples;
};

bool ledOn = false;
uint32_t lastAboveThresholdMs = 0;
uint32_t lastPrintMs = 0;

void setLed(bool on) {
  ledOn = on;

#if USE_NEOPIXEL_LED
  // ESP32-S3-DevKitC-1 uses an addressable onboard RGB LED on GPIO48.
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

float estimateDb(const SoundReading &reading) {
  const float rms = max(reading.rmsCounts, 1.0f);
  return 70.0f + (20.0f * log10f(rms / RMS_AT_70_DB));
}

void printReading(const SoundReading &reading, float db) {
  Serial.print("raw min=");
  Serial.print(reading.minRaw);
  Serial.print(" max=");
  Serial.print(reading.maxRaw);
  Serial.print(" p2p=");
  Serial.print(reading.maxRaw - reading.minRaw);
  Serial.print(" rms=");
  Serial.print(reading.rmsCounts, 1);
  Serial.print(" estimated_db=");
  Serial.print(db, 1);
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
  setLed(false);

  Serial.println();
  Serial.println("ESP32-S3 piezo sound threshold");
  Serial.println("Calibrate RMS_AT_70_DB for real dB behavior.");
}

void loop() {
  const SoundReading reading = readSoundWindow();
  const float db = estimateDb(reading);
  const uint32_t now = millis();

  if (db >= LED_ON_DB) {
    lastAboveThresholdMs = now;
    setLed(true);
  } else if (db <= LED_OFF_DB && (now - lastAboveThresholdMs) > LED_HOLD_MS) {
    setLed(false);
  }

  if ((now - lastPrintMs) >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printReading(reading, db);
  }
}
