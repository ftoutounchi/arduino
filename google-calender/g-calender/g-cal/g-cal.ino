#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>
#include <Wire.h>

// ================= USER DATA =================
const char* ssid = "Paradise"; // <--- ENTER YOUR WIFI NAME HERE
const char* password = "Arezoo&Farzad7";
const char* ics_url = "https://calendar.google.com/calendar/ical/27a537210e405162f136cb0f7d8d66d0a132005c7eafb3ece5b77de3d48ea3b2%40group.calendar.google.com/public/basic.ics";

// Timezone: 0 for UTC, 3600 for UTC+1, 12600 for Iran (UTC+3:30)
const long UTC_OFFSET_SECONDS = 0; 

// ================= HARDWARE =================
// OLED SSD1306 72x40 on ESP32-C3 (SDA=5, SCL=6)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS);

void setup() {
  Serial.begin(115200);
  Wire.begin(5, 6); 
  u8g2.begin();
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 25, "Connecting...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  timeClient.begin();
  Serial.println("\nWiFi Connected!");
}

void loop() {
  timeClient.update();
  
  // 1. Get today's date in YYYYMMDD format
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);
  char todayBuf[10];
  sprintf(todayBuf, "%04d%02d%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
  String todayDate = String(todayBuf);

  // 2. Prepare HTTPS Request
  NetworkClientSecure *client = new NetworkClientSecure;
  client->setInsecure(); 
  
  HTTPClient https;
  String eventTitle = "No events";

  if (https.begin(*client, ics_url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient* stream = https.getStreamPtr();
      bool isToday = false;

      // Stream data to save memory
      while (https.connected() && stream->available()) {
        String line = stream->readStringUntil('\n');
        line.trim();

        if (line.startsWith("DTSTART") && line.indexOf(todayDate) != -1) {
            isToday = true;
        }
        
        if (isToday && line.startsWith("SUMMARY:")) {
            eventTitle = line.substring(8);
            isToday = false; 
            break; // Stop after finding the first event for today
        }
        if (line.startsWith("END:VEVENT")) isToday = false;
      }
    }
    https.end();
  }
  delete client;

  // 3. Display Result
  u8g2.clearBuffer();
  
  // Draw Top Bar (Clock)
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 7, timeClient.getFormattedTime().c_str());
  u8g2.drawHLine(0, 9, 72);

  // Draw Event Body
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 22, "TODAY:");
  u8g2.setCursor(0, 34);
  if (eventTitle.length() > 11) {
    u8g2.print(eventTitle.substring(0, 11));
  } else {
    u8g2.print(eventTitle);
  }
  
  u8g2.sendBuffer();

  delay(600000); // Update every 10 minutes
}
