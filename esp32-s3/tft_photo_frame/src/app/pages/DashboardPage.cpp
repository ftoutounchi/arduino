#include "app/pages/DashboardPage.h"

DashboardPage::DashboardPage(LvglHost& host,
                             WeatherService& weatherService,
                             IPageNavigator& navigator)
    : view_(host, weatherService),
      navigator_(navigator) {}

void DashboardPage::onEnter() {
  view_.onEnter();
}

void DashboardPage::onExit() {
  view_.onExit();
}

void DashboardPage::update(uint32_t nowMs) {
  view_.update(nowMs);
}

void DashboardPage::render() {
  view_.render();
}

void DashboardPage::handleEvent(const PageEvent& event) {
  (void)view_.handleEvent(event);

  if (event.type == PageEvent::Type::kBootShortPress) {
    navigator_.requestSwitch(Id::kCalendar);
    return;
  }

  if (event.type == PageEvent::Type::kBootLongPress) {
    navigator_.requestPush(Id::kSettings);
  }
}
