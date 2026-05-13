#pragma once

#include "ui/LvglHost.h"
#include "ui/views/dashboard/DashboardView.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"
#include "app/services/WeatherService.h"

class DashboardPage : public IPage {
 public:
  DashboardPage(LvglHost& host,
                WeatherService& weatherService,
                IPageNavigator& navigator);

  Id id() const override { return Id::kDashboard; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

 private:
  DashboardView view_;
  IPageNavigator& navigator_;
};
