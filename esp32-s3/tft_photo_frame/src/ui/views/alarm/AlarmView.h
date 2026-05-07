#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "app/services/AlarmScheduler.h"
#include "config/AppConfig.h"
#include "ui/LvglHost.h"
#include "ui/views/IView.h"

class AlarmView : public IView {
 public:
  AlarmView(LvglHost& host, AlarmScheduler& alarmScheduler);

  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  ViewAction handleEvent(const PageEvent& event) override;

 private:
  static constexpr uint8_t kAlarmRows = Config::kMaxAlarms;
  static constexpr uint32_t kRefreshIntervalMs = 1000;

  LvglHost& host_;
  AlarmScheduler& alarmScheduler_;

  bool uiReady_;
  uint32_t lastRefreshMs_;
  lv_obj_t* screen_;
  lv_obj_t* lblTitle_;
  lv_obj_t* lblActive_;
  lv_obj_t* lblRows_[kAlarmRows];
  lv_obj_t* popupCard_;
  lv_obj_t* lblPopupBadge_;
  lv_obj_t* lblPopupTime_;
  lv_obj_t* lblPopupLabel_;
  lv_obj_t* lblPopupHint_;
  lv_obj_t* lblHint1_;
  lv_obj_t* lblHint2_;

  void ensureUi();
  void createUi();
  void refreshUi();
  static void weekdayMaskToText(uint8_t mask, char* out, size_t outSize);
};
