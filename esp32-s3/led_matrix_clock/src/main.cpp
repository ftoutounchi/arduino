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

const uint8_t DIGIT_WIDTH = 3;
const uint8_t DIGIT_HEIGHT = 5;
const uint8_t COLON_WIDTH = 1;
const int CHAR_SPACING = 1;
const int TEXT_BASELINE_Y = 1;
const unsigned long SCROLL_INTERVAL_MS = 130;
const unsigned long TIME_REFRESH_MS = 1000;

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

const uint8_t colonFont[DIGIT_HEIGHT] = {
  0b0, 0b1, 0b0, 0b1, 0b0
};

uint32_t timeColor;
uint32_t bgColor;
char currentTimeText[6] = "00:00";
int scrollX = MATRIX_WIDTH;
unsigned long lastScrollMs = 0;
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

int glyphWidth(char c) {
  if (c >= '0' && c <= '9') return DIGIT_WIDTH;
  if (c == ':') return COLON_WIDTH;
  return 0;
}

bool glyphPixelOn(char c, int row, int col) {
  if (row < 0 || row >= DIGIT_HEIGHT) return false;
  if (c >= '0' && c <= '9') {
    uint8_t rowBits = digitFont[c - '0'][row];
    return rowBits & (1 << (DIGIT_WIDTH - 1 - col));
  }
  if (c == ':') {
    return colonFont[row] & 0b1;
  }
  return false;
}

void drawGlyph(char c, int offsetX, int offsetY, uint32_t color) {
  int width = glyphWidth(c);
  for (int row = 0; row < DIGIT_HEIGHT; row++) {
    for (int col = 0; col < width; col++) {
      setPixel(offsetX + col, offsetY + row, glyphPixelOn(c, row, col) ? color : bgColor);
    }
  }
}

int textPixelWidth(const char* text) {
  int width = 0;
  for (int i = 0; text[i] != '\0'; i++) {
    width += glyphWidth(text[i]);
    if (text[i + 1] != '\0') width += CHAR_SPACING;
  }
  return width;
}

void drawScrollingText(const char* text, int startX, int y, uint32_t color) {
  clearMatrix();
  int cursorX = startX;
  for (int i = 0; text[i] != '\0'; i++) {
    drawGlyph(text[i], cursorX, y, color);
    cursorX += glyphWidth(text[i]) + CHAR_SPACING;
  }
  strip.show();
}

bool updateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return false;
  }

  snprintf(currentTimeText, sizeof(currentTimeText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
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

  if (updateTimeString()) {
    scrollX = MATRIX_WIDTH;
    drawScrollingText(currentTimeText, scrollX, TEXT_BASELINE_Y, timeColor);
    Serial.printf("Scrolling local time: %s\n", currentTimeText);
  } else {
    fillMatrix(strip.Color(100, 0, 0));
  }
}

void loop() {
  unsigned long nowMs = millis();

  if (nowMs - lastTimeRefreshMs >= TIME_REFRESH_MS) {
    lastTimeRefreshMs = nowMs;
    if (!updateTimeString()) {
      fillMatrix(strip.Color(100, 0, 0));
      delay(250);
      return;
    }
  }

  if (nowMs - lastScrollMs >= SCROLL_INTERVAL_MS) {
    lastScrollMs = nowMs;
    int width = textPixelWidth(currentTimeText);
    drawScrollingText(currentTimeText, scrollX, TEXT_BASELINE_Y, timeColor);
    scrollX--;
    if (scrollX < -width) {
      scrollX = MATRIX_WIDTH;
    }
  }
}
