#pragma once

#include "ui/LvglHost.h"
#include "ui/views/calendar/CalendarView.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"

class CalendarPage : public IPage {
 public:
  CalendarPage(LvglHost& host, IPageNavigator& navigator);

  Id id() const override { return Id::kCalendar; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

 private:
  CalendarView view_;
  IPageNavigator& navigator_;
};
