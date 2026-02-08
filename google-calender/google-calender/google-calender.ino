#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>

// ================= WiFi =================
const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";

// ================= OLED =================
#define SDA_PIN 5
#define SCL_PIN 6
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ================= HTTPS =================
WiFiClientSecure client;

// ================= Google Calendar ICS =================
const char* GCAL_HOST = "calendar.google.com";
const char* GCAL_PATH =
  "/calendar/ical/27a537210e405162f136cb0f7d8d66d0a132005c7eafb3ece5b77de3d48ea3b2%40group.calendar.google.com/public/basic.ics";

// ================= Timezone (Europe/Berlin) =================
const char* TZ_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3"; // Europe/Berlin

// ================= BOOT button =================
// BOOT_PIN is already defined by the ESP32 core for your board.
const unsigned long BOOT_DEBOUNCE_MS = 250;
unsigned long lastBootPressMs = 0;

// ================= Calendar State =================
time_t nextStart = 0;
time_t nextEnd   = 0;
String nextTitle = "";
portMUX_TYPE calMux = portMUX_INITIALIZER_UNLOCKED;

// blink control
const unsigned long BLINK_MS = 600;
bool showBigClock = true;
unsigned long lastBlinkToggle = 0;

// dismiss state: dismiss applies to a specific event (by its DTSTART)
bool eventDismissed = false;
time_t dismissedEventStart = 0;

// ================= Helpers =================
String hhmm(time_t t) {
  struct tm lt {};
  localtime_r(&t, &lt);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
  return String(buf);
}

String truncStr(const String& s, int len) {
  return s.length() <= len ? s : s.substring(0, len);
}

// ================= Display =================
void drawBigClock(const String& t) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_logisoso20_tf);
  u8g2.drawStr(0, 32, t.c_str());
  u8g2.sendBuffer();
}

void drawEventPage(const String& t, const String& title) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  // Title only, as large as possible
  String msg = title.length() ? title : "(no title)";
  msg.trim();
  if (!msg.length()) msg = "(no title)";

  const int16_t W = 72;
  const int16_t H = 40;

  if (msg.length() <= 4) {
    u8g2.setFont(u8g2_font_logisoso20_tf);
    int16_t x = (W - u8g2.getStrWidth(msg.c_str())) / 2;
    int16_t y = 30; // baseline
    u8g2.drawStr(x < 0 ? 0 : x, y, msg.c_str());
  } else if (msg.length() <= 7) {
    u8g2.setFont(u8g2_font_logisoso16_tf);
    int16_t x = (W - u8g2.getStrWidth(msg.c_str())) / 2;
    int16_t y = 28; // baseline
    u8g2.drawStr(x < 0 ? 0 : x, y, msg.c_str());
  } else {
    // Small font, try 2 lines
    u8g2.setFont(u8g2_font_8x13_tr);
    String l1 = msg;
    String l2 = "";
    const int maxLen = 9; // fits 72px width in 8x13
    if (msg.length() > maxLen) {
      int split = msg.lastIndexOf(' ', maxLen);
      if (split < 0) split = maxLen;
      l1 = msg.substring(0, split);
      l2 = msg.substring(split);
      l2.trim();
    }
    int16_t x1 = (W - u8g2.getStrWidth(l1.c_str())) / 2;
    int16_t y1 = l2.length() ? 18 : 26;
    u8g2.drawStr(x1 < 0 ? 0 : x1, y1, l1.c_str());
    if (l2.length()) {
      int16_t x2 = (W - u8g2.getStrWidth(l2.c_str())) / 2;
      int16_t y2 = 34;
      u8g2.drawStr(x2 < 0 ? 0 : x2, y2, l2.c_str());
    }
  }

  u8g2.sendBuffer();
}

void drawStatus(const String& l1, const String& l2 = "") {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 14, l1.c_str());
  if (l2.length()) u8g2.drawStr(0, 30, l2.c_str());
  u8g2.sendBuffer();
}

bool fetchNextEvent();

// ================= timegm replacement =================
time_t timegm_portable(struct tm* tm) {
  String oldTZ = getenv("TZ") ? getenv("TZ") : "";
  setenv("TZ", "UTC0", 1);
  tzset();
  time_t t = mktime(tm);
  if (oldTZ.length()) setenv("TZ", oldTZ.c_str(), 1);
  else unsetenv("TZ");
  tzset();
  return t;
}

bool parseIcsDate(const String& dt, time_t& out) {
  if (dt.length() < 8) return false;

  struct tm tmv {};
  tmv.tm_year = dt.substring(0,4).toInt() - 1900;
  tmv.tm_mon  = dt.substring(4,6).toInt() - 1;
  tmv.tm_mday = dt.substring(6,8).toInt();

  if (dt.length() >= 15 && dt.charAt(8) == 'T') {
    tmv.tm_hour = dt.substring(9,11).toInt();
    tmv.tm_min  = dt.substring(11,13).toInt();
    tmv.tm_sec  = dt.substring(13,15).toInt();
  }

  // Let libc determine DST for local-time conversions
  tmv.tm_isdst = -1;

  out = dt.endsWith("Z") ? timegm_portable(&tmv) : mktime(&tmv);
  return out > 0;
}

// ================= Calendar Fetch =================
bool fetchNextEvent() {
  client.setInsecure();
  if (!client.connect(GCAL_HOST, 443)) return false;

  client.printf(
    "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
    GCAL_PATH, GCAL_HOST
  );

  // Skip headers
  while (client.connected()) {
    if (client.readStringUntil('\n') == "\r") break;
  }

  time_t now = time(nullptr);
  bool inEvent = false;
  String ds, de, sum;
  time_t bestS = 0, bestE = 0;
  String bestT;
  bool foundActive = false;
  time_t activeS = 0, activeE = 0;
  String activeT;

  while (client.available()) {
    String l = client.readStringUntil('\n');
    l.trim();

    if (l == "BEGIN:VEVENT") { inEvent = true; ds=""; de=""; sum=""; }
    else if (l == "END:VEVENT") {
      time_t s, e;
      if (parseIcsDate(ds, s)) {
        if (!parseIcsDate(de, e)) e = s + 3600; // fallback 1h if DTEND missing
        if (now >= s && now <= e) {
          // Choose the active event that ends soonest
          if (!foundActive || e < activeE) {
            foundActive = true;
            activeS = s;
            activeE = e;
            activeT = sum;
          }
        } else if (s > now && (bestS == 0 || s < bestS)) {
          bestS = s; bestE = e; bestT = sum;
        }
      }
      inEvent = false;
    }
    else if (l.startsWith("DTSTART")) ds = l.substring(l.indexOf(':')+1);
    else if (l.startsWith("DTEND"))   de = l.substring(l.indexOf(':')+1);
    else if (l.startsWith("SUMMARY:")) sum = l.substring(8);
  }

  client.stop();
  if (!foundActive && !bestS) return false;

  // Update event atomically
  portENTER_CRITICAL(&calMux);
  if (foundActive) {
    nextStart = activeS;
    nextEnd   = activeE;
    nextTitle = activeT.length() ? activeT : "(no title)";
  } else {
    nextStart = bestS;
    nextEnd   = bestE;
    nextTitle = bestT.length() ? bestT : "(no title)";
  }

  // IMPORTANT: pulling calendar must NOT reset dismiss unless the event changed
  if (eventDismissed && dismissedEventStart != 0 && nextStart != dismissedEventStart) {
    eventDismissed = false;
    dismissedEventStart = 0;
  }
  portEXIT_CRITICAL(&calMux);

  return true;
}

// ================= BOOT handling =================
void handleBoot() {
  if (digitalRead(BOOT_PIN) == LOW) {
    unsigned long nowMs = millis();
    if (nowMs - lastBootPressMs > BOOT_DEBOUNCE_MS) {
      lastBootPressMs = nowMs;

      // Dismiss THIS specific event (do not clear nextStart/nextEnd)
      dismissedEventStart = nextStart;
      eventDismissed = true;

      // Immediately show time page
      showBigClock = true;
      lastBlinkToggle = nowMs;
    }
  }
}

// ================= Setup =================
void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  drawStatus("BOOT", "WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  drawStatus("NTP", "Sync...");
  // Configure SNTP with local timezone (handles CET/CEST automatically)
  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");

  // Don't block forever if NTP is unreachable
  const unsigned long NTP_TIMEOUT_MS = 20000;
  unsigned long startMs = millis();
  while (time(nullptr) < 1700000000 && (millis() - startMs) < NTP_TIMEOUT_MS) {
    delay(200);
  }
  if (time(nullptr) < 1700000000) {
    drawStatus("NTP FAIL", "CONTINUE");
    delay(1200);
  }

  fetchNextEvent(); // pull calendar only at boot
}

// ================= Loop =================
void loop() {
  handleBoot();

  time_t now = time(nullptr);
  String nowStr = hhmm(now);

  time_t ns = 0, ne = 0;
  bool dismissed = false;
  String title;
  portENTER_CRITICAL(&calMux);
  ns = nextStart;
  ne = nextEnd;
  title = nextTitle;
  dismissed = eventDismissed;
  portEXIT_CRITICAL(&calMux);

  // Auto-release when event ends (no auto-refresh)
  if (ne > 0 && now > ne) {
    portENTER_CRITICAL(&calMux);
    eventDismissed = false;
    dismissedEventStart = 0;
    portEXIT_CRITICAL(&calMux);

    showBigClock = true;
    lastBlinkToggle = millis();
  }

  // Event is active only if NOT dismissed and within the time window
  bool eventActive =
    !dismissed &&
    ns > 0 &&
    now >= ns &&
    now <= ne;

  // Default page: BIG TIME only
  if (!eventActive) {
    drawBigClock(nowStr);
    delay(150);
    return;
  }

  // When event is active: blink between big time page and event page
  if (millis() - lastBlinkToggle > BLINK_MS) {
    showBigClock = !showBigClock;
    lastBlinkToggle = millis();
  }

  if (showBigClock) drawBigClock(nowStr);
  else drawEventPage(nowStr, title);

  delay(80);
}
