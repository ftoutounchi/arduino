#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
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
const unsigned long TIME_REFRESH_MS = 1000;
const unsigned long SCROLL_STEP_MS = 180;
const unsigned long WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;
const unsigned long WEATHER_RETRY_MS = 30UL * 1000UL;
const unsigned long WEATHER_SHOW_MS = 4500;
const unsigned long TEMP_SHOW_MS = 3500;
const float WEATHER_LAT = 53.5991f;   // Hamburg
const float WEATHER_LON = 10.0267f;   // Hamburg

// 3x5 font, each row stored in low 3 bits.
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

const uint8_t colonFont[DIGIT_HEIGHT] = {0b0, 0b1, 0b0, 0b1, 0b0};
const uint8_t cFont[DIGIT_HEIGHT] = {0b111, 0b100, 0b100, 0b100, 0b111};
const uint8_t minusFont[DIGIT_HEIGHT] = {0b000, 0b000, 0b111, 0b000, 0b000};
const uint8_t oFont[DIGIT_HEIGHT] = {0b000, 0b111, 0b101, 0b101, 0b111};
const uint8_t starFont[DIGIT_HEIGHT] = {0b101, 0b010, 0b111, 0b010, 0b101};

uint32_t timeColor;
uint32_t bgColor;
char currentTimeText[6] = "00:00";
int scrollOffsetX = MATRIX_WIDTH;
unsigned long lastScrollStepMs = 0;
unsigned long lastTimeRefreshMs = 0;
unsigned long weatherShownSinceMs = 0;
unsigned long lastWeatherFetchMs = 0;
bool weatherValid = false;
int weatherCode = 0;
bool isDay = true;
float weatherTempC = 0.0f;
char tempText[10] = "--*o*";
int tempScrollOffsetX = MATRIX_WIDTH;
unsigned long tempShownSinceMs = 0;

enum DisplayMode {
  SHOW_TIME,
  SHOW_WEATHER,
  SHOW_TEMP
};

DisplayMode displayMode = SHOW_TIME;

int pixelIndex(int x, int y) {
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) {
    return -1;
  }
  int mappedY = MATRIX_HEIGHT - 1 - y;
  return mappedY * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
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

void drawCloud(int ox, int oy, uint32_t color) {
  const int cloudPixels[][2] = {
      {1, 1}, {2, 0}, {3, 0}, {4, 1}, {5, 1},
      {0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2},
      {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}
  };
  for (size_t i = 0; i < sizeof(cloudPixels) / sizeof(cloudPixels[0]); i++) {
    setPixel(ox + cloudPixels[i][0], oy + cloudPixels[i][1], color);
  }
}

void drawSun(int ox, int oy, uint32_t sunColor) {
  const int core[][2] = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
  const int rays[][2] = {{1, 0}, {2, 0}, {1, 3}, {2, 3}, {0, 1}, {0, 2}, {3, 1}, {3, 2}};
  for (size_t i = 0; i < sizeof(core) / sizeof(core[0]); i++) setPixel(ox + core[i][0], oy + core[i][1], sunColor);
  for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); i++) setPixel(ox + rays[i][0], oy + rays[i][1], sunColor);
}

void drawRain(int ox, int oy, uint32_t cloudColor, uint32_t rainColor) {
  drawCloud(ox, oy, cloudColor);
  setPixel(ox + 1, oy + 5, rainColor);
  setPixel(ox + 3, oy + 6, rainColor);
  setPixel(ox + 5, oy + 5, rainColor);
}

void drawThunder(int ox, int oy, uint32_t cloudColor, uint32_t boltColor) {
  drawCloud(ox, oy, cloudColor);
  setPixel(ox + 3, oy + 4, boltColor);
  setPixel(ox + 2, oy + 5, boltColor);
  setPixel(ox + 3, oy + 5, boltColor);
  setPixel(ox + 2, oy + 6, boltColor);
}

void showWeatherIcon() {
  clearMatrix();

  uint32_t yellow = strip.Color(180, 140, 0);
  uint32_t blue = strip.Color(0, 60, 180);
  uint32_t cloud = strip.Color(80, 80, 90);
  uint32_t white = strip.Color(120, 120, 120);
  uint32_t magenta = strip.Color(120, 0, 120);

  // WMO categories: clear(0,1), cloudy(2,3,45,48), rain(51-67,80-82), snow(71-77,85,86), thunder(95+)
  if (weatherCode == 0 || weatherCode == 1) {
    drawSun(2, 2, yellow);
  } else if ((weatherCode >= 2 && weatherCode <= 3) || weatherCode == 45 || weatherCode == 48) {
    drawCloud(1, 2, white);
  } else if ((weatherCode >= 51 && weatherCode <= 67) || (weatherCode >= 80 && weatherCode <= 82)) {
    drawRain(1, 1, cloud, blue);
  } else if ((weatherCode >= 71 && weatherCode <= 77) || weatherCode == 85 || weatherCode == 86) {
    drawCloud(1, 1, white);
    setPixel(2, 6, white);
    setPixel(4, 6, white);
  } else if (weatherCode >= 95) {
    drawThunder(1, 1, cloud, magenta);
  } else {
    drawCloud(1, 2, white);
  }

  strip.show();
}

int glyphWidth(char c) {
  if (c >= '0' && c <= '9') return DIGIT_WIDTH;
  if (c == ':') return COLON_WIDTH;
  if (c == 'C' || c == '-' || c == 'o' || c == '*') return DIGIT_WIDTH;
  return 0;
}

bool glyphPixelOn(char c, int row, int col) {
  if (row < 0 || row >= DIGIT_HEIGHT) return false;
  if (c >= '0' && c <= '9') {
    uint8_t rowBits = digitFont[c - '0'][row];
    return rowBits & (1 << (DIGIT_WIDTH - 1 - col));
  }
  if (c == ':') {
    return (col == 0) && (colonFont[row] & 0b1);
  }
  if (c == 'C') {
    return cFont[row] & (1 << (DIGIT_WIDTH - 1 - col));
  }
  if (c == '-') {
    return minusFont[row] & (1 << (DIGIT_WIDTH - 1 - col));
  }
  if (c == 'o') {
    return oFont[row] & (1 << (DIGIT_WIDTH - 1 - col));
  }
  if (c == '*') {
    return starFont[row] & (1 << (DIGIT_WIDTH - 1 - col));
  }
  return false;
}

void drawGlyph(char c, int offsetX, int offsetY, uint32_t color) {
  int width = glyphWidth(c);
  for (int row = 0; row < DIGIT_HEIGHT; row++) {
    for (int col = 0; col < width; col++) {
      bool on = glyphPixelOn(c, row, col);
      setPixel(offsetX + col, offsetY + row, on ? color : bgColor);
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

void showScrollingTime(const char* text, int offsetX) {
  clearMatrix();
  int cursorX = offsetX;
  for (int i = 0; text[i] != '\0'; i++) {
    drawGlyph(text[i], cursorX, 1, timeColor);
    cursorX += glyphWidth(text[i]) + CHAR_SPACING;
  }
  strip.show();
}

void showCenteredText(const char* text, uint32_t color) {
  clearMatrix();
  int width = textPixelWidth(text);
  int startX = (MATRIX_WIDTH - width) / 2;
  int cursorX = startX;
  for (int i = 0; text[i] != '\0'; i++) {
    drawGlyph(text[i], cursorX, 1, color);
    cursorX += glyphWidth(text[i]) + CHAR_SPACING;
  }
  strip.show();
}

uint32_t weatherThemeColor() {
  if (weatherCode == 0 || weatherCode == 1) return strip.Color(180, 140, 0); // sunny yellow
  if ((weatherCode >= 2 && weatherCode <= 3) || weatherCode == 45 || weatherCode == 48) return strip.Color(120, 120, 120); // cloud gray
  if ((weatherCode >= 51 && weatherCode <= 67) || (weatherCode >= 80 && weatherCode <= 82)) return strip.Color(0, 60, 180); // rain blue
  if ((weatherCode >= 71 && weatherCode <= 77) || weatherCode == 85 || weatherCode == 86) return strip.Color(160, 160, 180); // snow
  if (weatherCode >= 95) return strip.Color(120, 0, 120); // thunder magenta
  return strip.Color(140, 0, 0);
}

bool updateTimeText() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return false;
  }

  snprintf(currentTimeText, sizeof(currentTimeText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return true;
}

int extractIntAfterKey(const String& payload, const char* key, int fallback) {
  int searchFrom = 0;
  while (true) {
    int keyPos = payload.indexOf(key, searchFrom);
    if (keyPos < 0) return fallback;

    int colon = payload.indexOf(':', keyPos);
    if (colon < 0) return fallback;

    int i = colon + 1;
    while (i < payload.length() && (payload[i] == ' ' || payload[i] == '\n' || payload[i] == '\r')) i++;

    int sign = 1;
    if (i < payload.length() && payload[i] == '-') {
      sign = -1;
      i++;
    }

    int value = 0;
    bool hasDigit = false;
    while (i < payload.length() && isdigit(payload[i])) {
      hasDigit = true;
      value = value * 10 + (payload[i] - '0');
      i++;
    }

    if (hasDigit) {
      return sign * value;
    }

    searchFrom = keyPos + 1;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Weather fetch skipped: WiFi not connected");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  char url[220];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=weather_code,is_day,temperature_2m&timezone=auto",
           WEATHER_LAT, WEATHER_LON);

  if (!http.begin(client, url)) {
    Serial.println("Weather begin failed");
    return false;
  }
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Weather GET failed: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  int newCode = extractIntAfterKey(payload, "\"weather_code\"", -1);
  if (newCode < 0) {
    newCode = extractIntAfterKey(payload, "\"weathercode\"", -1);
  }
  int dayFlag = extractIntAfterKey(payload, "\"is_day\"", 1);
  int tempRaw = extractIntAfterKey(payload, "\"temperature_2m\"", 0);
  if (newCode < 0) {
    Serial.println("Weather parse failed: weather_code missing");
    Serial.printf("Payload head: %.140s\n", payload.c_str());
    return false;
  }

  weatherCode = newCode;
  isDay = (dayFlag == 1);
  weatherTempC = static_cast<float>(tempRaw);
  snprintf(tempText, sizeof(tempText), "%d*o*", static_cast<int>(weatherTempC));
  weatherValid = true;
  lastWeatherFetchMs = millis();
  Serial.printf("Weather updated: code=%d, isDay=%d, temp=%s\n", weatherCode, isDay ? 1 : 0, tempText);
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
  fetchWeather();

  if (updateTimeText()) {
    scrollOffsetX = MATRIX_WIDTH;
    showScrollingTime(currentTimeText, scrollOffsetX);
    Serial.printf("Scrolling local time (R->L): %s\n", currentTimeText);
  } else {
    fillMatrix(strip.Color(100, 0, 0));
  }
}

void loop() {
  unsigned long nowMs = millis();

  if (nowMs - lastTimeRefreshMs >= TIME_REFRESH_MS) {
    lastTimeRefreshMs = nowMs;
    if (!updateTimeText()) {
      fillMatrix(strip.Color(100, 0, 0));
      delay(250);
      return;
    }
  }

  unsigned long weatherInterval = weatherValid ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS;
  if (nowMs - lastWeatherFetchMs >= weatherInterval) {
    fetchWeather();
  }

  if (displayMode == SHOW_TIME) {
    if (nowMs - lastScrollStepMs >= SCROLL_STEP_MS) {
      lastScrollStepMs = nowMs;
      showScrollingTime(currentTimeText, scrollOffsetX);
      scrollOffsetX--;
      int width = textPixelWidth(currentTimeText);
      if (scrollOffsetX < -width) {
        scrollOffsetX = MATRIX_WIDTH;
        displayMode = SHOW_WEATHER;
        weatherShownSinceMs = nowMs;
      }
    }
  } else {
    if (displayMode == SHOW_WEATHER) {
      showWeatherIcon();
      if (nowMs - weatherShownSinceMs >= WEATHER_SHOW_MS) {
        displayMode = SHOW_TEMP;
        tempShownSinceMs = nowMs;
      }
    } else {
      char tempValueText[6];
      snprintf(tempValueText, sizeof(tempValueText), "%d", static_cast<int>(weatherTempC));
      showCenteredText(tempValueText, strip.Color(0, 0, 180));
      if (nowMs - tempShownSinceMs >= TEMP_SHOW_MS) {
        displayMode = SHOW_TIME;
        scrollOffsetX = MATRIX_WIDTH;
      }
    }
  }
}
