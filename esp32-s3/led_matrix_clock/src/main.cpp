#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <Adafruit_NeoPixel.h>

#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_PIXELS (MATRIX_WIDTH * MATRIX_HEIGHT)

#ifndef LED_PIN
#define LED_PIN 6
#endif

#define BRIGHTNESS 45

const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
const char* timezone = "CET-1CEST,M3.5.0/2,M10.5.0/3";

Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

const uint8_t DIGIT_WIDTH = 5;
const uint8_t DIGIT_HEIGHT = 7;
const unsigned long TIME_REFRESH_MS = 1000;
const unsigned long SCROLL_STEP_MS = 180;

// 5x7 font, each row stored in low 5 bits.
const uint8_t digitFont[10][DIGIT_HEIGHT] = {
  {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}, // 0
  {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
  {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}, // 2
  {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}, // 3
  {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}, // 4
  {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}, // 5
  {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}, // 6
  {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, // 7
  {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}, // 8
  {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b11100}  // 9
};

uint32_t timeColor;
uint32_t bgColor;
char currentDigits[5] = "0000";
int activeDigitIndex = 0;
int digitOffsetX = MATRIX_WIDTH;
unsigned long lastScrollStepMs = 0;
unsigned long lastTimeRefreshMs = 0;

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

void showScrollingDigit(char digitChar, int offsetX) {
  clearMatrix();
  int digit = digitChar - '0';
  drawDigit(digit, offsetX, 0, timeColor);
  strip.show();
}

bool updateTimeDigits() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return false;
  }

  snprintf(currentDigits, sizeof(currentDigits), "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return true;
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

  timeColor = strip.Color(140, 0, 0);
  bgColor = strip.Color(0, 0, 0);
  fillMatrix(strip.Color(20, 0, 0));

  connectWiFi();
  fillMatrix(strip.Color(35, 0, 0));
  setupTime();

  if (updateTimeDigits()) {
    activeDigitIndex = 0;
    digitOffsetX = MATRIX_WIDTH;
    showScrollingDigit(currentDigits[activeDigitIndex], digitOffsetX);
    Serial.printf("Scrolling local digits (R->L): %c %c %c %c\n",
                  currentDigits[0], currentDigits[1], currentDigits[2], currentDigits[3]);
  } else {
    fillMatrix(strip.Color(100, 0, 0));
  }
}

void loop() {
  unsigned long nowMs = millis();

  if (nowMs - lastTimeRefreshMs >= TIME_REFRESH_MS) {
    lastTimeRefreshMs = nowMs;
    if (!updateTimeDigits()) {
      fillMatrix(strip.Color(100, 0, 0));
      delay(250);
      return;
    }
  }

  if (nowMs - lastScrollStepMs >= SCROLL_STEP_MS) {
    lastScrollStepMs = nowMs;
    showScrollingDigit(currentDigits[activeDigitIndex], digitOffsetX);
    digitOffsetX--;

    if (digitOffsetX < -static_cast<int>(DIGIT_WIDTH)) {
      digitOffsetX = MATRIX_WIDTH;
      activeDigitIndex = (activeDigitIndex + 1) % 4;
    }
  }
}
