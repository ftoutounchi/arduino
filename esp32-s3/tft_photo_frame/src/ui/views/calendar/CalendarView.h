#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "ui/LvglHost.h"
#include "ui/views/IView.h"

class CalendarView : public IView {
 public:
  explicit CalendarView(LvglHost& host);

  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  ViewAction handleEvent(const PageEvent& event) override;

 private:
  LvglHost& host_;

  bool uiReady_;
  bool needImmediateRefresh_;
  uint32_t lastRefreshMs_;

  lv_obj_t* screen_;
  lv_obj_t* lblTitle_;
  lv_obj_t* lblMonth_;
  lv_obj_t* table_;
  lv_obj_t* lblHint_;

  void ensureUi();
  void createUi();
  void resetTable();
  void updateCalendarText();
};
