#include "infra/network/GoogleScriptAgendaProvider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config/AppConfig.h"

GoogleScriptAgendaProvider::GoogleScriptAgendaProvider(WifiManager& wifi) : wifi_(wifi) {}

bool GoogleScriptAgendaProvider::fetchEvents(Event* outEvents,
                                             uint8_t maxEvents,
                                             uint8_t* outCount,
                                             String* outErrorText) {
  if (outCount != nullptr) {
    *outCount = 0;
  }
  if (outErrorText != nullptr) {
    *outErrorText = "";
  }

  if (outEvents == nullptr || outCount == nullptr || maxEvents == 0) {
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda Error";
    }
    return false;
  }

  if (!wifi_.ensureConnected()) {
    if (outErrorText != nullptr) {
      *outErrorText = "Wi-Fi Offline";
    }
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(Config::kAgendaHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, Config::gAgendaScriptUrl)) {
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda HTTP";
    }
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda Error";
    }
    return false;
  }

  const String payload = http.getString();
  http.end();
  if (payload.length() == 0) {
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda Empty";
    }
    return false;
  }

  DynamicJsonDocument doc(4096);
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Agenda JSON parse error: %s\n", err.c_str());
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda JSON";
    }
    return false;
  }

  if (!doc.is<JsonArray>()) {
    if (outErrorText != nullptr) {
      *outErrorText = "Agenda Format";
    }
    return false;
  }

  JsonArray events = doc.as<JsonArray>();
  uint8_t written = 0;
  for (JsonObject event : events) {
    if (written >= maxEvents) {
      break;
    }

    outEvents[written].title = event["title"] | "(No title)";
    outEvents[written].startIso = event["start"] | "";
    ++written;
  }

  *outCount = written;
  return true;
}
