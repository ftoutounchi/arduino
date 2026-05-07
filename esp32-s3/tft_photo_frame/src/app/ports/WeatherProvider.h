#pragma once

#include <Arduino.h>

class IWeatherProvider {
 public:
  static constexpr uint8_t kMaxForecastDays = 4;

  struct ForecastDay {
    char isoDate[11];
    int weatherCode;
    bool hasWeatherCode;
    float tempMaxC;
    bool hasTempMax;
  };

  struct Snapshot {
    float tempC;
    bool hasTemp;
    int weatherCode;
    bool hasWeatherCode;
    int humidityPct;
    bool hasHumidity;
    int pressureHpa;
    bool hasPressure;
    float windMs;
    bool hasWind;
    ForecastDay forecast[kMaxForecastDays];
    uint8_t forecastCount;
  };

  virtual ~IWeatherProvider() = default;

  virtual bool fetchSnapshot(Snapshot* outSnapshot, String* outErrorText) = 0;
};
