#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <vector>
#include "LittleFS.h"

// ==========================================
// 1. SETTINGS & STATIC CONTENT
// ==========================================
const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
const long gmtOffset_sec = 3600; 
const int  daylightOffset_sec = 0; 

#define PIN_SDA 5
#define PIN_SCL 6
#define PIN_BUTTON 9 

// Move static HTML/CSS to Flash memory (PROGMEM) to save RAM
const char HTML_HEAD[] PROGMEM = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>"
"body{font-family:sans-serif;padding:10px;background:#f4f4f4} .c{background:#fff;padding:12px;margin-bottom:10px;border-radius:8px;box-shadow:0 1px 3px #ccc}"
"input,button{width:100%;padding:10px;margin:5px 0} .d-box{display:inline-block;width:14%;text-align:center;font-size:12px}"
"button{background:#5e2ca5;color:#fff;border:none;border-radius:4px;font-weight:bold}</style></head><body><h3>Alarms</h3>";

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
WebServer server(80);

struct Alarm { 
  int id; 
  int h, m; 
  String msg; 
  bool days[7]; 
};
std::vector<Alarm> alarms;
int nextId = 1;
String activeMsg = "";

// ==========================================
// 2. OPTIMIZED STORAGE (sscanf is much faster)
// ==========================================
void save() {
  File f = LittleFS.open("/a.txt", "w");
  if(!f) return;
  for(const auto &a : alarms) {
    f.printf("%d|%d|%d|", a.id, a.h, a.m);
    for(int i=0; i<7; i++) f.print(a.days[i] ? '1' : '0');
    f.printf("|%s\n", a.msg.c_str());
  }
  f.close();
}

void load() {
  if(!LittleFS.exists("/a.txt")) return;
  File f = LittleFS.open("/a.txt", "r");
  char line[128];
  while(f.available()){
    int l = f.readBytesUntil('\n', line, sizeof(line)-1);
    line[l] = '\0';
    
    Alarm a;
    char dStr[8];
    char mStr[32];
    // Parsing with sscanf is more memory-efficient than String.substring
    if(sscanf(line, "%d|%d|%d|%7[^|]|%31[^\n]", &a.id, &a.h, &a.m, dStr, mStr) == 5) {
      for(int i=0; i<7; i++) a.days[i] = (dStr[i] == '1');
      a.msg = String(mStr);
      alarms.push_back(a);
      if(a.id >= nextId) nextId = a.id + 1;
    }
  }
  f.close();
}

// ==========================================
// 3. UI & SERVER
// ==========================================
void handleRoot() {
  String h;
  h.reserve(3000); // Increased reserve for the extra day strings
  h = FPSTR(HTML_HEAD);
  
  const char* dayAbbr[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

  for(const auto &a : alarms) {
    // 1. Build the Days string: [Su Mo ...]
    String activeDays = "[";
    bool first = true;
    for(int i=0; i<7; i++) {
      if(a.days[i]) {
        if(!first) activeDays += " ";
        activeDays += dayAbbr[i];
        first = false;
      }
    }
    activeDays += "]";

    // 2. Format the display line
    char buf[256];
    snprintf(buf, sizeof(buf), 
             "<div class='c'><span style='color:#5e2ca5;font-size:12px;margin-right:5px'>%s</span> "
             "<b>%02d:%02d</b> %s "
             "<a href='/del?id=%d' style='color:red;float:right;font-size:12px'>DEL</a></div>", 
             activeDays.c_str(), a.h, a.m, a.msg.c_str(), a.id);
    h += buf;
  }

  h += "<div class='c'><form action='/add'>Msg:<input name='m' required maxlength='12'>Time:<input name='t' type='time' required><div style='margin:10px 0'>Days:<br>";
  const char* dNames[] = {"S","M","T","W","T","F","S"};
  for(int i=0; i<7; i++) {
    h += "<div class='d-box'>"; h += dNames[i]; h += "<br><input type='checkbox' name='d"; h += i; h += "' checked style='width:auto'></div>";
  }
  h += "</div><button>SAVE</button></form></div></body></html>";
  server.send(200, "text/html", h);
}

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL); 
  Wire.setClock(400000);
  u8g2.begin(); 
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  if(!LittleFS.begin(true)) return; 
  load();

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(100);
  
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  
  server.on("/", handleRoot);
  server.on("/add", [](){
    server.sendHeader("Connection", "close"); // make web faster 
    String t = server.arg("t");
    Alarm na; 
    na.id = nextId++; 
    na.h = t.substring(0,2).toInt(); 
    na.m = t.substring(3,5).toInt(); 
    na.msg = server.arg("m");
    for(int i=0; i<7; i++) na.days[i] = server.hasArg("d" + String(i));
    alarms.push_back(na); 
    save(); 
    server.sendHeader("Location","/"); 
    server.send(303);
  });
  server.on("/del", [](){
    int id = server.arg("id").toInt();
    for(auto it = alarms.begin(); it != alarms.end(); ++it) {
      if(it->id == id) { alarms.erase(it); break; }
    }
    save(); 
    server.sendHeader("Location","/"); 
    server.send(303);
  });
  server.begin();
}

void loop() {
  server.handleClient();
  
  static unsigned long lastCheck = 0;
  static int lastMin = -1;
  static bool blink = false;

  // Run display/logic update every 500ms
  if(millis() - lastCheck < 500) return;
  lastCheck = millis();
  blink = !blink;

  struct tm ti;
  if(getLocalTime(&ti)) {
    // Check Alarms
    bool anyActive = false;
    for(const auto &a : alarms) {
      if(a.h == ti.tm_hour && a.m == ti.tm_min && a.days[ti.tm_wday]) {
        activeMsg = a.msg;
        anyActive = true;
      }
    }
    
    if(digitalRead(PIN_BUTTON) == LOW) activeMsg = "";

    // ONLY REDRAW if minute changed OR an alarm is active (for blinking)
    if(ti.tm_min != lastMin || activeMsg != "") {
      lastMin = ti.tm_min;
      u8g2.clearBuffer();

      if(activeMsg == "") {
        char ts[10], ds[22];
        sprintf(ts, "%02d:%02d", ti.tm_hour, ti.tm_min);
        strftime(ds, 22, "%m/%d       %a", &ti);
        u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(2, 9, ds);
        u8g2.setFont(u8g2_font_logisoso20_tn); u8g2.drawStr(1, 38, ts);
      } else {
        if(blink) {
          u8g2.setFont(u8g2_font_9x15_tf);
          int x = (72 - u8g2.getStrWidth(activeMsg.c_str())) / 2;
          u8g2.drawStr(max(0, x), 26, activeMsg.c_str());
        } else {
           char ts[10];
           sprintf(ts, "%02d:%02d", ti.tm_hour, ti.tm_min);
           u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(10, 9, "!!! ALARM !!!"); 
           u8g2.setFont(u8g2_font_logisoso20_tn); u8g2.drawStr(1, 38, ts);
        }
      }
      u8g2.sendBuffer();
    }
  }
}