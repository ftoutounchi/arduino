#include "infra/wifi/WifiManager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config/AppConfig.h"

bool WifiManager::connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(Config::kWifiSsid, Config::kWifiPassword);

  Serial.printf("Connecting to Wi-Fi SSID '%s'\n", Config::kWifiSsid);
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection failed");
    return false;
  }

  Serial.print("Wi-Fi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool WifiManager::ensureConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  Serial.println("Wi-Fi disconnected, reconnecting...");
  return connect();
}
