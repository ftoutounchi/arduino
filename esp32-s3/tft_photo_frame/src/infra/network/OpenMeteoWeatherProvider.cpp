#include "infra/network/OpenMeteoWeatherProvider.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <string.h>

#include "config/AppConfig.h"

namespace {
void copyIsoDate(char* dst, size_t dstSize, const char* src) {
  if (dst == nullptr || dstSize == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}
}  // namespace

OpenMeteoWeatherProvider::OpenMeteoWeatherProvider(WifiManager& wifi) : wifi_(wifi) {}

bool OpenMeteoWeatherProvider::fetchSnapshot(Snapshot* outSnapshot, String* outErrorText) {
  if (outErrorText != nullptr) {
    *outErrorText = "";
  }
  if (outSnapshot == nullptr) {
    if (outErrorText != nullptr) {
      *outErrorText = "Weather Error";
    }
    return false;
  }
  *outSnapshot = Snapshot{};

  if (!wifi_.ensureConnected()) {
    if (outErrorText != nullptr) {
      *outErrorText = "Wi-Fi Offline";
    }
    return false;
  }

  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(Config::gWeatherLatitude, 4);
  url += "&longitude=";
  url += String(Config::gWeatherLongitude, 4);
  url += "&current=temperature_2m,weather_code,relative_humidity_2m,pressure_msl,wind_speed_10m";
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
  url += "&forecast_days=";
  url += String(static_cast<unsigned>(kMaxForecastDays));
  url += "&wind_speed_unit=ms&timezone=auto";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(Config::kWeatherHttpTimeoutMs);
  if (!http.begin(client, url)) {
    if (outErrorText != nullptr) {
      *outErrorText = "Weather HTTP";
    }
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    if (outErrorText != nullptr) {
      *outErrorText = "Weather Error";
    }
    return false;
  }

  const String payload = http.getString();
  http.end();
  if (payload.length() == 0) {
    if (outErrorText != nullptr) {
      *outErrorText = "Empty Payload";
    }
    return false;
  }

  DynamicJsonDocument doc(4096);
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Weather JSON parse error: %s\n", err.c_str());
    Serial.println(payload.substring(0, 220));
    if (outErrorText != nullptr) {
      *outErrorText = "JSON Error";
    }
    return false;
  }

  if ((doc["error"] | false) == true) {
    const char* reason = doc["reason"] | "Weather API error";
    Serial.printf("Weather API error: %s\n", reason);
    if (outErrorText != nullptr) {
      *outErrorText = "API Error";
    }
    return false;
  }

  const float temp = doc["current"]["temperature_2m"] | NAN;
  if (isnan(temp)) {
    if (outErrorText != nullptr) {
      *outErrorText = "Temp Missing";
    }
    return false;
  }

  const int code = doc["current"]["weather_code"] | -1;
  const int humidity = doc["current"]["relative_humidity_2m"] | -1;
  const int pressure = doc["current"]["pressure_msl"] | -1;
  const float wind = doc["current"]["wind_speed_10m"] | NAN;

  outSnapshot->tempC = temp;
  outSnapshot->hasTemp = true;
  outSnapshot->weatherCode = code;
  outSnapshot->hasWeatherCode = (code >= 0);
  outSnapshot->humidityPct = humidity;
  outSnapshot->hasHumidity = (humidity >= 0);
  outSnapshot->pressureHpa = pressure;
  outSnapshot->hasPressure = (pressure >= 0);
  outSnapshot->windMs = wind;
  outSnapshot->hasWind = !isnan(wind);

  JsonArrayConst dayTime = doc["daily"]["time"].as<JsonArrayConst>();
  JsonArrayConst dayMax = doc["daily"]["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst dayCode = doc["daily"]["weather_code"].as<JsonArrayConst>();
  if (!dayTime.isNull() && !dayMax.isNull()) {
    for (uint8_t i = 0; i < kMaxForecastDays; ++i) {
      if (i >= dayTime.size() || i >= dayMax.size()) {
        break;
      }

      IWeatherProvider::ForecastDay& day = outSnapshot->forecast[outSnapshot->forecastCount];
      const char* iso = dayTime[i] | "";
      copyIsoDate(day.isoDate, sizeof(day.isoDate), iso);

      const float tMax = dayMax[i] | NAN;
      if (!isnan(tMax)) {
        day.tempMaxC = tMax;
        day.hasTempMax = true;
      }

      int dailyCode = -1;
      if (!dayCode.isNull() && i < dayCode.size()) {
        dailyCode = dayCode[i] | -1;
      }
      day.weatherCode = dailyCode;
      day.hasWeatherCode = (dailyCode >= 0);

      ++outSnapshot->forecastCount;
    }
  }

  return true;
}
