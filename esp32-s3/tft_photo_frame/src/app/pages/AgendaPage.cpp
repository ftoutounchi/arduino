#include "app/pages/AgendaPage.h"

AgendaPage::AgendaPage(LvglHost& host, AgendaService& agendaService, IPageNavigator& navigator)
    : view_(host, agendaService), navigator_(navigator) {}

void AgendaPage::onEnter() {
  view_.onEnter();
}

void AgendaPage::onExit() {
  view_.onExit();
}

void AgendaPage::update(uint32_t nowMs) {
  view_.update(nowMs);
}

void AgendaPage::render() {
  view_.render();
}

void AgendaPage::handleEvent(const PageEvent& event) {
  (void)view_.handleEvent(event);

  if (event.type == PageEvent::Type::kBootShortPress) {
    navigator_.requestSwitch(Id::kAlarm);
    return;
  }

  if (event.type == PageEvent::Type::kBootLongPress) {
    navigator_.requestPush(Id::kSettings);
  }
}
