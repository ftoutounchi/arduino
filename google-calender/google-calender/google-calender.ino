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

// ================= Timezone =================
const char* TZ_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// ================= BOOT button =================
// BOOT_PIN is already defined by the ESP32 core for your board.
const unsigned long BOOT_DEBOUNCE_MS = 250;
unsigned long lastBootPressMs = 0;

// ================= Calendar State =================
time_t nextStart = 0;
time_t nextEnd   = 0;
String nextTitle = "";

// refresh control
const unsigned long CAL_REFRESH_MS = 10UL * 60UL * 1000UL;          // calendar pull interval
const unsigned long AFTER_EVENT_REFETCH_COOLDOWN_MS = 15000;        // after-event cooldown

unsigned long lastCalFetch = 0;
unsigned long lastAfterEventFetch = 0;

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

  // Time (top)
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, t.c_str());

  // Title 
  u8g2.setFont(u8g2_font_8x13_tr);
  u8g2.drawStr(0, 36, truncStr(title, 9).c_str());

  u8g2.sendBuffer();
}

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

  while (client.available()) {
    String l = client.readStringUntil('\n');
    l.trim();

    if (l == "BEGIN:VEVENT") { inEvent = true; ds=""; de=""; sum=""; }
    else if (l == "END:VEVENT") {
      time_t s, e;
      if (parseIcsDate(ds, s)) {
        if (!parseIcsDate(de, e)) e = s + 3600; // fallback 1h if DTEND missing
        if (s > now && (bestS == 0 || s < bestS)) {
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
  if (!bestS) return false;

  // Update event
  nextStart = bestS;
  nextEnd   = bestE;
  nextTitle = bestT.length() ? bestT : "(no title)";

  // IMPORTANT: pulling calendar must NOT reset dismiss unless the event changed
  if (eventDismissed && dismissedEventStart != 0 && nextStart != dismissedEventStart) {
    eventDismissed = false;
    dismissedEventStart = 0;
  }

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

      // Optionally fetch next event soon (cooldown protected) - useful if event is long
      if (nowMs - lastAfterEventFetch > AFTER_EVENT_REFETCH_COOLDOWN_MS) {
        fetchNextEvent();
        lastAfterEventFetch = nowMs;
        lastCalFetch = nowMs; // reset periodic timer too
      }
    }
  }
}

// ================= Setup =================
void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  setenv("TZ", TZ_INFO, 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  while (time(nullptr) < 1700000000) delay(200);

  fetchNextEvent();
  lastCalFetch = millis();
}

// ================= Loop =================
void loop() {
  handleBoot();

  time_t now = time(nullptr);
  String nowStr = hhmm(now);

  // Periodic calendar refresh
  if (millis() - lastCalFetch > CAL_REFRESH_MS) {
    fetchNextEvent();
    lastCalFetch = millis();
  }

  // Auto-release when event ends: fetch the next event
  if (nextEnd > 0 && now > nextEnd) {
    if (millis() - lastAfterEventFetch > AFTER_EVENT_REFETCH_COOLDOWN_MS) {
      fetchNextEvent();
      lastAfterEventFetch = millis();

      // event ended -> any previous dismiss is no longer relevant
      eventDismissed = false;
      dismissedEventStart = 0;

      showBigClock = true;
      lastBlinkToggle = millis();
    }
  }

  // Event is active only if NOT dismissed and within the time window
  bool eventActive =
    !eventDismissed &&
    nextStart > 0 &&
    now >= nextStart &&
    now <= nextEnd;

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
  else drawEventPage(nowStr, nextTitle);

  delay(80);
}
