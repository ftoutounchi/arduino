#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "app/services/WeatherService.h"
#include "config/AppConfig.h"
#include "ui/LvglHost.h"
#include "ui/views/IView.h"

class DashboardView : public IView {
 public:
  DashboardView(LvglHost& host, WeatherService& weatherService);

  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  ViewAction handleEvent(const PageEvent& event) override;

 private:
  static constexpr uint8_t kForecastSlots = 4;
  static constexpr uint16_t kWeatherIconSize = 88;
  static constexpr uint16_t kForecastIconSize = 48;

  LvglHost& host_;
  WeatherService& weatherService_;

  bool uiReady_;
  bool needImmediateRefresh_;
  uint32_t lastClockUpdateMs_;
  uint32_t lastWeatherUpdateMs_;
  uint32_t lastWeatherAttemptMs_;

  lv_obj_t* screen_;
  lv_obj_t* lblTitle_;
  lv_obj_t* lblDate_;
  lv_obj_t* lblYear_;
  lv_obj_t* lblTime_;
  lv_obj_t* weatherIconImg_;
  lv_obj_t* lblLocation_;
  lv_obj_t* lblCondition_;
  lv_obj_t* lblTemp_;
  lv_obj_t* lblHumidity_;
  lv_obj_t* lblPressure_;
  lv_obj_t* lblWind_;
  lv_obj_t* lblForecastDay_[kForecastSlots];
  lv_obj_t* forecastIconImg_[kForecastSlots];
  lv_obj_t* lblForecastTemp_[kForecastSlots];
  lv_obj_t* lblHint_;

  void ensureUi();
  void createUi();

  void updateClockLabel();
  void updateWeatherLabel();
  void setHint(const char* text);
  void clearForecast();
  void drawWeatherIcon(int code, bool night);
};
