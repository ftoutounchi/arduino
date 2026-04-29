#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <Adafruit_NeoPixel.h>

#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_PIXELS (MATRIX_WIDTH * MATRIX_HEIGHT)

#ifndef LED_PIN
#define LED_PIN 21
#endif

#define BRIGHTNESS 45

const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
const char* timezone = "CET-1CEST,M3.5.0/2,M10.5.0/3";

Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

const uint8_t DIGIT_WIDTH = 3;
const uint8_t DIGIT_HEIGHT = 5;

// Each byte is one row, stored in the low 3 bits.
const uint8_t digitFont[10][DIGIT_HEIGHT] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

uint32_t hourColor;
uint32_t bgColor;
int lastHour = -1;

int pixelIndex(int x, int y) {
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) {
    return -1;
  }
  if (y % 2 == 0) {
    return y * MATRIX_WIDTH + x;
  }
  return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
}

void setPixel(int x, int y, uint32_t color) {
  int index = pixelIndex(x, y);
  if (index >= 0) {
    strip.setPixelColor(index, color);
  }
}

void clearMatrix() {
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, 0);
  }
}

void fillMatrix(uint32_t color) {
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void drawDigit(int digit, int offsetX, int offsetY, uint32_t color) {
  if (digit < 0 || digit > 9) return;
  for (int row = 0; row < DIGIT_HEIGHT; row++) {
    uint8_t rowBits = digitFont[digit][row];
    for (int col = 0; col < DIGIT_WIDTH; col++) {
      bool on = rowBits & (1 << (DIGIT_WIDTH - 1 - col));
      setPixel(offsetX + col, offsetY + row, on ? color : bgColor);
    }
  }
}

void showHour(int hour) {
  clearMatrix();
  int tens = hour / 10;
  int ones = hour % 10;
  int offsetY = 1;

  if (tens == 0) {
    drawDigit(ones, 2, offsetY, hourColor);
  } else {
    drawDigit(tens, 0, offsetY, hourColor);
    drawDigit(ones, 4, offsetY, hourColor);
  }
  strip.show();
}

bool waitForTime() {
  const unsigned long timeoutMs = 20000;
  unsigned long started = millis();
  while (millis() - started < timeoutMs) {
    if (time(nullptr) > 1700000000) {
      return true;
    }
    delay(250);
  }
  return false;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    Serial.print('.');
    delay(500);
    retry++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed.");
  }
}

void setupTime() {
  timeval resetTime = {};
  settimeofday(&resetTime, nullptr);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", timezone, 1);
  tzset();
  if (waitForTime()) {
    Serial.println("Time synchronized.");
  } else {
    Serial.println("Failed to sync time.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("LED matrix clock starting on GPIO %d\n", LED_PIN);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  hourColor = strip.Color(0, 120, 200);
  bgColor = strip.Color(0, 0, 0);
  fillMatrix(strip.Color(0, 0, 35));

  connectWiFi();
  fillMatrix(strip.Color(35, 20, 0));
  setupTime();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1000)) {
    lastHour = timeinfo.tm_hour;
    showHour(lastHour);
    Serial.printf("Showing local hour: %02d\n", lastHour);
  } else {
    fillMatrix(strip.Color(100, 0, 0));
  }
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    if (timeinfo.tm_hour != lastHour) {
      lastHour = timeinfo.tm_hour;
      showHour(lastHour);
      Serial.printf("Hour updated: %02d\n", lastHour);
    }
  }
  delay(60000);
}
