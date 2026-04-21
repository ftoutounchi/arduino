#include <WiFi.h>
#include <time.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <vector>
#include "LittleFS.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Weather Settings for Hamburg 22177
const char* lat = "53.60";
const char* lon = "10.08";

bool showWeather = false;
unsigned long weatherStartTime = 0;

float weatherTemp[5] = {0};     // 0 = current, 1..4 = next days
int weatherCode[5] = {0};
int weatherViewIndex = 0;       // 0=current, 1=tomorrow, ... max 4
int weatherDaysLoaded = 0;      // how many day entries are valid

// ==========================================
// 1. SETTINGS
// ==========================================
const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define TYPE_ODD_EVEN  0
#define TYPE_ODD       1
#define TYPE_EVEN      2

#define PIN_SDA 5
#define PIN_SCL 6
#define PIN_BUTTON 9

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);

struct Alarm {
  uint32_t id       : 10;
  uint32_t h        : 5;
  uint32_t m        : 6;
  uint32_t sun :1, mon :1, tue :1, wed :1, thu :1, fri :1, sat :1;
  uint32_t weekType : 2;
  char msg[16];
};

std::vector<Alarm> alarms;
int nextId = 1;
String activeMsg = "";
int lastTriggeredMin = -1;

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat) +
               "&longitude=" + String(lon) +
               "&current_weather=true"
               "&daily=weathercode,temperature_2m_max"
               "&timezone=auto";
  http.begin(url);

  if (http.GET() == HTTP_CODE_OK) {
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, http.getString());

    if (!err) {
      // current weather
      weatherTemp[0] = doc["current_weather"]["temperature"] | 0.0;
      weatherCode[0] = doc["current_weather"]["weathercode"] | 0;

      // next 4 days
      JsonArray temps = doc["daily"]["temperature_2m_max"].as<JsonArray>();
      JsonArray codes = doc["daily"]["weathercode"].as<JsonArray>();

      weatherDaysLoaded = 1; // current always available

      for (int i = 1; i <= 4; i++) {
        if (temps.size() > i && codes.size() > i) {
          weatherTemp[i] = temps[i].as<float>();
          weatherCode[i] = codes[i].as<int>();
          weatherDaysLoaded++;
        } else {
          break;
        }
      }
    }
  }

  http.end();
}

void drawWeatherIcon(int x, int y, int code) {
  u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
  char icon_char = 'E';
  if (code == 0) icon_char = 'E';
  else if (code <= 3) icon_char = 'A';
  else if (code <= 48) icon_char = 'B';
  else if (code <= 67) icon_char = 'C';
  else if (code <= 77) icon_char = 'D';
  else if (code <= 82) icon_char = 'C';
  else icon_char = 'M';
  u8g2.drawGlyph(x, y, icon_char);
}

// ==========================================
// 2. STORAGE
// ==========================================

void addAlarm(int h, int m, const char* mtxt, std::vector<int> days, int type) {
  Alarm a;
  a.id = nextId++;
  a.h = h;
  a.m = m;

  a.sun = (days.size() > 0) ? days[0] : 0;
  a.mon = (days.size() > 1) ? days[1] : 0;
  a.tue = (days.size() > 2) ? days[2] : 0;
  a.wed = (days.size() > 3) ? days[3] : 0;
  a.thu = (days.size() > 4) ? days[4] : 0;
  a.fri = (days.size() > 5) ? days[5] : 0;
  a.sat = (days.size() > 6) ? days[6] : 0;

  a.weekType = type;
  strncpy(a.msg, mtxt, 15);
  a.msg[15] = '\0';
  alarms.push_back(a);
}

// ==========================================
// 3. CORE LOGIC
// ==========================================
void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "Starting...");
  u8g2.sendBuffer();

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if (!LittleFS.begin(false)) {
    u8g2.drawStr(0, 24, "FS Mount Fail");
    u8g2.sendBuffer();
    LittleFS.begin(true);
  }

  if (alarms.empty()) {
    addAlarm(6, 0,  "Thyroid",  {1,1,1,1,1,1,1}, TYPE_ODD_EVEN);
    addAlarm(10, 00, "Farzad V", {1,1,1,1,1,1,1}, TYPE_ODD_EVEN);
    addAlarm(14, 0, "Arezoo V", {1,1,1,1,1,1,1}, TYPE_ODD_EVEN);
    addAlarm(18, 0, "Gelb S",   {0,1,1,0,0,0,0}, TYPE_EVEN);
  }

  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "WiFi Connect...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  int wifiRetry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetry < 20) {
    delay(500);
    wifiRetry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawStr(0, 24, "Syncing Time...");
    u8g2.sendBuffer();

    configTzTime(TZ_INFO, "time.google.com", "pool.ntp.org");

    struct tm timeinfo;
    int ntpRetry = 0;
    while (!getLocalTime(&timeinfo) && ntpRetry < 15) {
      delay(1000);
      ntpRetry++;
    }
  } else {
    u8g2.drawStr(0, 24, "WiFi Failed");
    u8g2.sendBuffer();
    delay(2000);
  }
}

bool dayMatches(const Alarm& a, int wday) {
  switch (wday) {
    case 0: return a.sun;
    case 1: return a.mon;
    case 2: return a.tue;
    case 3: return a.wed;
    case 4: return a.thu;
    case 5: return a.fri;
    case 6: return a.sat;
    default: return false;
  }
}

bool weekMatches(const Alarm& a, const tm& ti) {
  if (a.weekType == TYPE_ODD_EVEN) return true;

  char wk[4];
  strftime(wk, sizeof(wk), "%V", &ti);
  int isoWeek = atoi(wk);
  bool isOdd = (isoWeek % 2) != 0;

  if (a.weekType == TYPE_ODD)  return isOdd;
  if (a.weekType == TYPE_EVEN) return !isOdd;
  return false;
}

void loop() {
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastButtonTime = 0;
  static bool blinkState = false;
  static int lastCheckedMinuteKey = -1;

  if (digitalRead(PIN_BUTTON) == LOW && millis() - lastButtonTime > 250) {
    lastButtonTime = millis();

    if (activeMsg == "") {
      if (!showWeather) {
        fetchWeather();
        weatherViewIndex = 0;
        showWeather = true;
        weatherStartTime = millis();
      } else {
        weatherViewIndex++;
        if (weatherViewIndex >= weatherDaysLoaded || weatherViewIndex > 4) {
          weatherViewIndex = 0;
        }
        weatherStartTime = millis(); // reset 10s timeout on every press
      }
    } else {
      // Alarm is active -> dismiss only by button press
      activeMsg = "";
      showWeather = false;
    }
  }

  if (showWeather && (millis() - weatherStartTime > 10000)) {
    showWeather = false;
    weatherViewIndex = 0;
  }

  struct tm ti;
  if (!getLocalTime(&ti, 100)) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 20, "No time");
    u8g2.sendBuffer();
    return;
  }

  int h = ti.tm_hour;
  int m = ti.tm_min;

  int minuteKey = ti.tm_yday * 1440 + h * 60 + m;

  if (minuteKey != lastCheckedMinuteKey) {
    lastCheckedMinuteKey = minuteKey;

    // Only trigger a new alarm if none is currently active
    if (activeMsg == "") {
      for (const auto& a : alarms) {
        if (a.h == h &&
            a.m == m &&
            dayMatches(a, ti.tm_wday) &&
            weekMatches(a, ti)) {
          activeMsg = a.msg;
          showWeather = false;
          break;
        }
      }
    }
  }

  if (millis() - lastDisplayUpdate < 500) return;
  lastDisplayUpdate = millis();
  blinkState = !blinkState;

  u8g2.clearBuffer();

  if (showWeather) {
    drawWeatherIcon(0, 36, weatherCode[weatherViewIndex]);

    char tBuf[8];
    snprintf(tBuf, sizeof(tBuf), "%d", (int)weatherTemp[weatherViewIndex]);

    u8g2.setFont(u8g2_font_6x10_tf);
    if (weatherViewIndex == 0) {
      u8g2.drawStr(38, 10, "Now");
    } else {
      char dayBuf[8];
      snprintf(dayBuf, sizeof(dayBuf), "D+%d", weatherViewIndex);
      u8g2.drawStr(38, 10, dayBuf);
    }

    u8g2.setFont(u8g2_font_logisoso24_tn);
    u8g2.drawStr(38, 38, tBuf);
  }
  else if (activeMsg == "") {
    char buf[20];
    u8g2.setFont(u8g2_font_7x14_tr);
    strftime(buf, sizeof(buf), "%d.%m  %a", &ti);
    u8g2.drawStr(0, 10, buf);

    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    u8g2.setFont(u8g2_font_logisoso20_tn);
    int tw = u8g2.getStrWidth(buf);
    u8g2.drawStr((72 - tw) / 2, 40, buf);
  }
  else {
    if (blinkState) {
      u8g2.setFont(u8g2_font_9x15_tf);
      int x = (72 - u8g2.getStrWidth(activeMsg.c_str())) / 2;
      u8g2.drawStr(max(0, x), 26, activeMsg.c_str());
    } else {
      char buf[20];
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(5, 12, "!! ALARM !!");

      snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
      u8g2.setFont(u8g2_font_logisoso20_tn);
      int tw = u8g2.getStrWidth(buf);
      u8g2.drawStr((72 - tw) / 2, 40, buf);
    }
  }

  u8g2.sendBuffer();
}