#pragma once

#include <Arduino.h>

#include "app/ports/WeatherProvider.h"

class WeatherService {
 public:
  static constexpr uint8_t kMaxForecastDays = IWeatherProvider::kMaxForecastDays;

  struct Forecast {
    String dayLabel;
    int weatherCode;
    bool hasWeatherCode;
    int maxTempC;
    bool hasMaxTemp;
  };

  struct Result {
    bool success;
    String errorText;
    float tempC;
    int weatherCode;
    bool hasTemp;
    bool hasWeatherCode;
    int humidityPct;
    bool hasHumidity;
    int pressureHpa;
    bool hasPressure;
    float windMs;
    bool hasWind;
    Forecast forecast[kMaxForecastDays];
    uint8_t forecastCount;
  };

  explicit WeatherService(IWeatherProvider& provider);

  void fetch(Result* outResult) const;

 private:
  IWeatherProvider& provider_;

  static void dayLabelFromIsoDate(const char* isoDate, String* outDayLabel);
};
