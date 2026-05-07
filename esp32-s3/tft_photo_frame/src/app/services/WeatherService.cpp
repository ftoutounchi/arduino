#include "app/services/WeatherService.h"

#include <math.h>
#include <time.h>

WeatherService::WeatherService(IWeatherProvider& provider) : provider_(provider) {}

void WeatherService::fetch(Result* outResult) const {
  if (outResult == nullptr) {
    return;
  }

  outResult->success = false;
  outResult->errorText = "Weather Error";
  outResult->tempC = NAN;
  outResult->weatherCode = -1;
  outResult->hasTemp = false;
  outResult->hasWeatherCode = false;
  outResult->humidityPct = -1;
  outResult->hasHumidity = false;
  outResult->pressureHpa = -1;
  outResult->hasPressure = false;
  outResult->windMs = NAN;
  outResult->hasWind = false;
  outResult->forecastCount = 0;
  for (uint8_t i = 0; i < kMaxForecastDays; ++i) {
    outResult->forecast[i].dayLabel = "---";
    outResult->forecast[i].weatherCode = -1;
    outResult->forecast[i].hasWeatherCode = false;
    outResult->forecast[i].maxTempC = 0;
    outResult->forecast[i].hasMaxTemp = false;
  }

  IWeatherProvider::Snapshot snapshot = {};
  String errorText;
  if (!provider_.fetchSnapshot(&snapshot, &errorText)) {
    outResult->errorText = errorText.isEmpty() ? "Weather Error" : errorText;
    return;
  }

  outResult->success = true;
  outResult->tempC = snapshot.tempC;
  outResult->weatherCode = snapshot.weatherCode;
  outResult->hasTemp = snapshot.hasTemp;
  outResult->hasWeatherCode = snapshot.hasWeatherCode;
  outResult->humidityPct = snapshot.humidityPct;
  outResult->hasHumidity = snapshot.hasHumidity;
  outResult->pressureHpa = snapshot.pressureHpa;
  outResult->hasPressure = snapshot.hasPressure;
  outResult->windMs = snapshot.windMs;
  outResult->hasWind = snapshot.hasWind;

  const uint8_t limit = (snapshot.forecastCount <= kMaxForecastDays) ? snapshot.forecastCount : kMaxForecastDays;
  outResult->forecastCount = limit;
  for (uint8_t i = 0; i < limit; ++i) {
    const IWeatherProvider::ForecastDay& day = snapshot.forecast[i];
    dayLabelFromIsoDate(day.isoDate, &outResult->forecast[i].dayLabel);
    outResult->forecast[i].weatherCode = day.weatherCode;
    outResult->forecast[i].hasWeatherCode = day.hasWeatherCode;
    outResult->forecast[i].hasMaxTemp = day.hasTempMax;
    outResult->forecast[i].maxTempC = day.hasTempMax ? static_cast<int>(round(day.tempMaxC)) : 0;
  }
}

void WeatherService::dayLabelFromIsoDate(const char* isoDate, String* outDayLabel) {
  if (outDayLabel == nullptr) {
    return;
  }

  *outDayLabel = "---";
  if (isoDate == nullptr || isoDate[0] == '\0') {
    return;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  if (sscanf(isoDate, "%d-%d-%d", &year, &month, &day) != 3) {
    return;
  }

  struct tm tmDate = {};
  tmDate.tm_year = year - 1900;
  tmDate.tm_mon = month - 1;
  tmDate.tm_mday = day;
  tmDate.tm_isdst = -1;
  if (mktime(&tmDate) < 0) {
    return;
  }

  static const char* kShortDays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  const int weekday = tmDate.tm_wday;
  if (weekday < 0 || weekday > 6) {
    return;
  }

  *outDayLabel = kShortDays[weekday];
}
