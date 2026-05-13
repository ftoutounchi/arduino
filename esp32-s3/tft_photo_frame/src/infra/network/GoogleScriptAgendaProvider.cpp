#include "infra/network/GoogleScriptAgendaProvider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config/AppConfig.h"

namespace {
void trimInPlace(String* text) {
  if (text == nullptr) {
    return;
  }
  text->trim();
}

bool extractFieldFromLegacyEvent(const String& eventText,
                                 const char* key,
                                 String* outValue) {
  if (key == nullptr || outValue == nullptr) {
    return false;
  }

  const String marker = String(key) + "=";
  const int keyPos = eventText.indexOf(marker);
  if (keyPos < 0) {
    return false;
  }

  int valueStart = keyPos + static_cast<int>(marker.length());
  while (valueStart < eventText.length() && eventText[valueStart] == ' ') {
    ++valueStart;
  }

  int valueEnd = eventText.length();
  for (int i = valueStart; i < eventText.length(); ++i) {
    const char ch = eventText[i];
    if (ch == ',' || ch == '}') {
      valueEnd = i;
      break;
    }
  }

  *outValue = eventText.substring(valueStart, valueEnd);
  trimInPlace(outValue);
  return outValue->length() > 0;
}

uint8_t parseLegacyEventList(const String& payload, IAgendaProvider::Event* outEvents, uint8_t maxEvents) {
  if (outEvents == nullptr || maxEvents == 0) {
    return 0;
  }

  uint8_t written = 0;
  int cursor = 0;
  while (cursor < payload.length() && written < maxEvents) {
    const int openBrace = payload.indexOf('{', cursor);
    if (openBrace < 0) {
      break;
    }
    const int closeBrace = payload.indexOf('}', openBrace + 1);
    if (closeBrace < 0) {
      break;
    }

    const String eventChunk = payload.substring(openBrace + 1, closeBrace);
    String title;
    String start;
    if (!extractFieldFromLegacyEvent(eventChunk, "title", &title)) {
      title = "(No title)";
    }
    extractFieldFromLegacyEvent(eventChunk, "startDate", &start);
    if (start.length() == 0) {
      extractFieldFromLegacyEvent(eventChunk, "start", &start);
    }

    outEvents[written].title = title;
    outEvents[written].startIso = start;
    ++written;
    cursor = closeBrace + 1;
  }

  return written;
}
}  // namespace

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
    const uint8_t written = parseLegacyEventList(payload, outEvents, maxEvents);
    if (written > 0) {
      *outCount = written;
      return true;
    }

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
    const char* startText = event["start"] | event["startDate"] | event["date"] | "";
    outEvents[written].startIso = startText;
    ++written;
  }

  *outCount = written;
  return true;
}
