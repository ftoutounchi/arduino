#pragma once

#include <Arduino.h>

#include "app/ports/WeatherProvider.h"
#include "infra/wifi/WifiManager.h"

class OpenMeteoWeatherProvider : public IWeatherProvider {
 public:
  explicit OpenMeteoWeatherProvider(WifiManager& wifi);

  bool fetchSnapshot(Snapshot* outSnapshot, String* outErrorText) override;

 private:
  WifiManager& wifi_;
};
