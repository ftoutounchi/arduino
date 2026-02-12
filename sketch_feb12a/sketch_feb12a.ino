#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <vector>
#include "LittleFS.h"

// ==========================================
// 1. SETTINGS
// ==========================================
const char* ssid = "Paradise";
const char* password = "Arezoo&Farzad7";
long gmtOffset_sec = 3600; 
int  daylightOffset_sec = 0; 

#define PIN_SDA 5
#define PIN_SCL 6
#define PIN_BUTTON 9 

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
WebServer server(80);

struct Alarm { 
  int id; 
  int h, m; 
  String msg; 
  bool days[7]; // 0=Sun, 1=Mon...
};
std::vector<Alarm> alarms;
int nextId = 1;
String activeMsg = "";

// ==========================================
// 2. STORAGE
// ==========================================
void save() {
  File f = LittleFS.open("/a.txt", "w");
  for(auto &a : alarms) {
    f.printf("%d|%d|%d|", a.id, a.h, a.m);
    for(int i=0; i<7; i++) f.print(a.days[i] ? "1" : "0");
    f.printf("|%s\n", a.msg.c_str());
  }
  f.close();
}

void load() {
  if(!LittleFS.exists("/a.txt")) return;
  File f = LittleFS.open("/a.txt", "r");
  while(f.available()){
    String l = f.readStringUntil('\n');
    int p1 = l.indexOf('|'), p2 = l.indexOf('|', p1+1), p3 = l.indexOf('|', p2+1), p4 = l.indexOf('|', p3+1);
    if(p4 == -1) continue;
    Alarm a; a.id = l.substring(0,p1).toInt(); a.h = l.substring(p1+1,p2).toInt(); a.m = l.substring(p2+1,p3).toInt();
    String d = l.substring(p3+1, p4);
    for(int i=0; i<7; i++) a.days[i] = (d[i] == '1');
    a.msg = l.substring(p4+1);
    alarms.push_back(a);
    if(a.id >= nextId) nextId = a.id + 1;
  }
  f.close();
}

// ==========================================
// 3. UI
// ==========================================
void handleRoot() {
  String h = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
  h += "body{font-family:sans-serif;padding:10px;background:#f4f4f4} .c{background:#fff;padding:12px;margin-bottom:10px;border-radius:8px;box-shadow:0 1px 3px #ccc}";
  h += "input,button{width:100%;padding:10px;margin:5px 0} .d-box{display:inline-block;width:14%;text-align:center;font-size:12px}";
  h += "button{background:#5e2ca5;color:#fff;border:none;border-radius:4px;font-weight:bold}</style></head><body><h3>Alarms</h3>";
  
  for(auto &a : alarms) {
    h += "<div class='c'><b>"+String(a.h)+":"+String(a.m < 10 ? "0":"")+String(a.m)+"</b> "+a.msg;
    h += " <a href='/del?id="+String(a.id)+"' style='color:red;float:right;font-size:12px'>DEL</a></div>";
  }

  h += "<div class='c'><form action='/add'>Msg:<input name='m' required maxlength='12'>Time:<input name='t' type='time' required>";
  h += "<div style='margin:10px 0'>Days:<br>";
  const char* dNames[] = {"S","M","T","W","T","F","S"};
  for(int i=0; i<7; i++) h += "<div class='d-box'>"+String(dNames[i])+"<br><input type='checkbox' name='d"+String(i)+"' checked style='width:auto'></div>";
  h += "</div><button>SAVE</button></form></div></body></html>";
  server.send(200, "text/html", h);
}

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL); Wire.setClock(400000);
  u8g2.begin(); pinMode(PIN_BUTTON, INPUT_PULLUP);
  LittleFS.begin(true); load();
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(100);
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  server.on("/", handleRoot);
  server.on("/add", [](){
    String t = server.arg("t");
    Alarm na; na.id = nextId++; na.h = t.substring(0,2).toInt(); na.m = t.substring(3,5).toInt(); na.msg = server.arg("m");
    for(int i=0; i<7; i++) na.days[i] = server.hasArg("d" + String(i));
    alarms.push_back(na); save(); server.sendHeader("Location","/"); server.send(303);
  });
  server.on("/del", [](){
    int id = server.arg("id").toInt();
    for(int i=0; i<alarms.size(); i++) if(alarms[i].id == id) { alarms.erase(alarms.begin()+i); break; }
    save(); server.sendHeader("Location","/"); server.send(303);
  });
  server.begin();
}

void loop() {
  server.handleClient();
  static unsigned long lastUpd = 0;
  if(millis() - lastUpd < 500) return; 
  lastUpd = millis();

  struct tm ti;
  if(getLocalTime(&ti)) {
    bool anyActive = false;
    for(auto &a : alarms) {
      if(a.h == ti.tm_hour && a.m == ti.tm_min && a.days[ti.tm_wday]) {
        activeMsg = a.msg;
        anyActive = true;
      }
    }
    if(!anyActive && activeMsg != "") { /* Auto-clear if time passes */ }
    if(digitalRead(PIN_BUTTON) == LOW) activeMsg = "";

    u8g2.clearBuffer();
    if(activeMsg == "") {
      char ts[10], ds[22];
      sprintf(ts, "%02d:%02d", ti.tm_hour, ti.tm_min);
      strftime(ds, 22, "%m/%d       %a", &ti);
      u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(2, 9, ds);
      u8g2.setFont(u8g2_font_logisoso20_tn); u8g2.drawStr(1, 38, ts);
    } else {
      static bool b = false; b = !b;
      if(b) {
        u8g2.setFont(u8g2_font_9x15_tf);
        int x = (72 - u8g2.getStrWidth(activeMsg.c_str())) / 2;
        u8g2.drawStr(max(0, x), 26, activeMsg.c_str());
      } else {
        char ts[10];
         sprintf(ts, "%02d:%02d", ti.tm_hour, ti.tm_min);
         u8g2.setFont(u8g2_font_haxrcorp4089_tr);
         u8g2.drawStr(10, 9, "!!! ALARM !!!"); 
         u8g2.setFont(u8g2_font_logisoso20_tn);
         u8g2.drawStr(1, 38, ts);

      }
    }
    u8g2.sendBuffer();
  }
}