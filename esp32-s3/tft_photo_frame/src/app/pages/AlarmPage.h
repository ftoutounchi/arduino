#pragma once

#include "app/services/AlarmScheduler.h"
#include "ui/LvglHost.h"
#include "ui/views/alarm/AlarmView.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"

class AlarmPage : public IPage {
 public:
  AlarmPage(LvglHost& host, AlarmScheduler& alarmScheduler, IPageNavigator& navigator);

  Id id() const override { return Id::kAlarm; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

 private:
  AlarmScheduler& alarmScheduler_;
  AlarmView view_;
  IPageNavigator& navigator_;
};
