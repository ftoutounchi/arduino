#include "app/pages/CalendarPage.h"

CalendarPage::CalendarPage(LvglHost& host, IPageNavigator& navigator)
    : view_(host), navigator_(navigator) {}

void CalendarPage::onEnter() {
  view_.onEnter();
}

void CalendarPage::onExit() {
  view_.onExit();
}

void CalendarPage::update(uint32_t nowMs) {
  view_.update(nowMs);
}

void CalendarPage::render() {
  view_.render();
}

void CalendarPage::handleEvent(const PageEvent& event) {
  (void)view_.handleEvent(event);

  if (event.type == PageEvent::Type::kBootShortPress) {
    navigator_.requestSwitch(Id::kAgenda);
    return;
  }

  if (event.type == PageEvent::Type::kBootLongPress) {
    navigator_.requestPush(Id::kSettings);
  }
}
