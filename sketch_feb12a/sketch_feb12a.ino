#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <vector>
<<<<<<< HEAD
#include "LittleFS.h"

// ==========================================
// 1. SETTINGS & STATIC CONTENT
// ==========================================
const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
const long gmtOffset_sec = 3600; 
const int  daylightOffset_sec = 0; 

=======
#include <deque>
#include "LittleFS.h"

// ==========================================
// 1. CONFIGURATION
// ==========================================
const char* ssid     = "Paradise";
const char* password = "Arezoo&Farzad7";

long gmtOffset_sec = 0; 
int  daylightOffset_sec = 0; 

// ==========================================
// 2. HARDWARE PINS
// ==========================================
>>>>>>> 55ed0a9 (add alarm file)
#define PIN_SDA 5
#define PIN_SCL 6
#define PIN_BUTTON 9 

<<<<<<< HEAD
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
=======
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
WebServer server(80);

// ==========================================
// 3. LOGIC & STORAGE
// ==========================================
enum AlarmType { TYPE_ONCE, TYPE_DAILY, TYPE_WEEKLY, TYPE_MONTHLY };

struct AlarmRule {
  int id;
  AlarmType type;
  int hour, minute, dom;
  bool weekdays[7];
  String message;
};

std::vector<AlarmRule> rules;
std::deque<String> pendingQueue; 
int nextRuleId = 1;
int lastCheckedMinute = -1;
unsigned long lastBlink = 0;
bool blinkState = false;

void saveRules() {
  File f = LittleFS.open("/alarms.txt", "w");
  if (!f) return;
  for (auto &r : rules) {
    f.printf("%d|%d|%d|%d|%d|", r.id, (int)r.type, r.hour, r.minute, r.dom);
    for(int i=0; i<7; i++) f.print(r.weekdays[i] ? "1" : "0");
    f.printf("|%s\n", r.message.c_str());
>>>>>>> 55ed0a9 (add alarm file)
  }
  f.close();
}

<<<<<<< HEAD
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
=======
void loadRules() {
  if (!LittleFS.exists("/alarms.txt")) return;
  File f = LittleFS.open("/alarms.txt", "r");
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() < 10) continue;
    int p[6]; int cur = 0;
    for(int i=0; i<6; i++) { int next = line.indexOf('|', cur); p[i] = next; cur = next + 1; }
    AlarmRule r;
    r.id = line.substring(0, p[0]).toInt();
    r.type = (AlarmType)line.substring(p[0]+1, p[1]).toInt();
    r.hour = line.substring(p[1]+1, p[2]).toInt();
    r.minute = line.substring(p[2]+1, p[3]).toInt();
    r.dom = line.substring(p[3]+1, p[4]).toInt();
    String wd = line.substring(p[4]+1, p[5]);
    for(int i=0; i<7; i++) r.weekdays[i] = (wd[i] == '1');
    r.message = line.substring(p[5]+1);
    rules.push_back(r);
    if (r.id >= nextRuleId) nextRuleId = r.id + 1;
>>>>>>> 55ed0a9 (add alarm file)
  }
  f.close();
}

// ==========================================
<<<<<<< HEAD
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
=======
// 4. WEB INTERFACE
// ==========================================
String getPage() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;background:#f4f4f9;padding:15px;}.card{background:white;padding:12px;border-radius:8px;margin-bottom:15px;}input,select,button{width:100%;padding:10px;margin:5px 0;}.btn-del{background:#d9534f;color:white;width:auto;float:right;border:none;border-radius:4px;padding:5px;}</style>";
  html += "<script>function updateFields(v){document.getElementById('wd').style.display=(v=='2'?'block':'none');document.getElementById('md').style.display=(v=='3'?'block':'none');}</script></head><body>";
  html += "<h2>Reminder Box</h2><div class='card'><h3>Alarms</h3>";
  for(auto &r : rules) {
    html += "<div style='border-bottom:1px solid #eee;padding:8px 0;'><b>" + String(r.hour < 10 ? "0":"") + String(r.hour) + ":" + String(r.minute < 10 ? "0":"") + String(r.minute) + "</b> - " + r.message;
    html += " <a href='/del?id=" + String(r.id) + "'><button class='btn-del'>Del</button></a></div>";
  }
  html += "</div><div class='card'><h3>Add New</h3><form action='/add' method='POST'>Msg: <input name='msg' required maxlength='12'>Time: <input type='time' name='time' required>";
  html += "Type: <select name='type' onchange='updateFields(this.value)'><option value='1'>Daily</option><option value='0'>Once</option><option value='2'>Weekly</option><option value='3'>Monthly</option></select>";
  html += "<div id='wd' style='display:none'>Days: S<input type='checkbox' name='w0'> M<input type='checkbox' name='w1'> T<input type='checkbox' name='w2'> W<input type='checkbox' name='w3'> T<input type='checkbox' name='w4'> F<input type='checkbox' name='w5'> S<input type='checkbox' name='w6'></div>";
  html += "<div id='md' style='display:none'>Day: <input type='number' name='dom' min='1' max='31'></div><button type='submit' style='background:#5e2ca5;color:white;border:none;border-radius:4px;'>Save</button></form></div></body></html>";
  return html;
}

void handleAdd() {
  AlarmRule r; r.id = nextRuleId++; r.message = server.arg("msg");
  r.hour = server.arg("time").substring(0,2).toInt(); r.minute = server.arg("time").substring(3,5).toInt();
  int t = server.arg("type").toInt(); r.type = (AlarmType)t;
  if(t == 2) for(int i=0; i<7; i++) r.weekdays[i] = server.hasArg("w" + String(i));
  if(t == 3) r.dom = server.arg("dom").toInt();
  rules.push_back(r); saveRules(); server.sendHeader("Location", "/"); server.send(303);
}

void handleDel() {
  int id = server.arg("id").toInt();
  for (auto it = rules.begin(); it != rules.end(); ++it) if (it->id == id) { rules.erase(it); break; }
  saveRules(); server.sendHeader("Location", "/"); server.send(303);
}

void checkAlarms() {
  struct tm ti; if(!getLocalTime(&ti)) return;
  if(ti.tm_min == lastCheckedMinute) return;
  lastCheckedMinute = ti.tm_min;
  for (auto &r : rules) {
    if(r.hour == ti.tm_hour && r.minute == ti.tm_min) {
      bool trigger = false;
      if(r.type == TYPE_DAILY) trigger = true;
      else if(r.type == TYPE_WEEKLY && r.weekdays[ti.tm_wday]) trigger = true;
      else if(r.type == TYPE_MONTHLY && r.dom == ti.tm_mday) trigger = true;
      else if(r.type == TYPE_ONCE) trigger = true;
      if(trigger) {
        bool exists = false; for(auto &m : pendingQueue) if(m == r.message) exists = true;
        if(!exists) { if(pendingQueue.size() >= 5) pendingQueue.pop_front(); pendingQueue.push_back(r.message); }
      }
    }
  }
}

void setup() {
  Serial.begin(115200); pinMode(PIN_BUTTON, INPUT_PULLUP);
  LittleFS.begin(true); loadRules(); u8g2.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  server.on("/", [](){ server.send(200, "text/html", getPage()); });
  server.on("/add", handleAdd); server.on("/del", handleDel);
>>>>>>> 55ed0a9 (add alarm file)
  server.begin();
}

void loop() {
<<<<<<< HEAD
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
=======
  server.handleClient(); checkAlarms();
  if(digitalRead(PIN_BUTTON) == LOW) { if(!pendingQueue.empty()) { pendingQueue.pop_front(); delay(300); } }

  u8g2.clearBuffer();
  struct tm ti;
  if(getLocalTime(&ti)) {
    char tStr[10], dStr[20];
    sprintf(tStr, "%02d:%02d", ti.tm_hour, ti.tm_min);
    strftime(dStr, 20, "%m/%d  %a", &ti); // Double space and full Weekday

    if(pendingQueue.empty()) {
      // TOP LINE: Date and Weekday (Full Width)
      u8g2.setFont(u8g2_font_haxrcorp4089_tr); // Slightly larger, clear font
      u8g2.drawStr(2, 9, dStr); 

      // CENTER LINE: Big Clock
      u8g2.setFont(u8g2_font_logisoso20_tn); // Increased from 16 to 20
      u8g2.drawStr(1, 38, tStr); 
    } else {
      if(millis() - lastBlink > 500) { lastBlink = millis(); blinkState = !blinkState; }
      if(blinkState) {
         u8g2.setFont(u8g2_font_haxrcorp4089_tr);
         u8g2.drawStr(2, 9, "--- ALARM ---");
         u8g2.setFont(u8g2_font_logisoso20_tn);
         u8g2.drawStr(1, 38, tStr);
      } else {
         u8g2.setFont(u8g2_font_7x14_tf); // Slightly taller message font
         u8g2.drawStr(0, 26, pendingQueue.front().c_str());
      }
    }
  }
  u8g2.sendBuffer();
>>>>>>> 55ed0a9 (add alarm file)
}