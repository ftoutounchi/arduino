#include "app/pages/AlarmPage.h"

AlarmPage::AlarmPage(LvglHost& host, AlarmScheduler& alarmScheduler, IPageNavigator& navigator)
    : alarmScheduler_(alarmScheduler), view_(host, alarmScheduler), navigator_(navigator) {}

void AlarmPage::onEnter() {
  view_.onEnter();
}

void AlarmPage::onExit() {
  view_.onExit();
}

void AlarmPage::update(uint32_t nowMs) {
  view_.update(nowMs);
}

void AlarmPage::render() {
  view_.render();
}

void AlarmPage::handleEvent(const PageEvent& event) {
  (void)view_.handleEvent(event);

  if (event.type == PageEvent::Type::kBootShortPress) {
    alarmScheduler_.acknowledgeActiveAlert();
    navigator_.requestSwitch(Id::kPhotoFrame);
    return;
  }

  if (event.type == PageEvent::Type::kBootLongPress && !alarmScheduler_.hasActiveAlert()) {
    navigator_.requestPush(Id::kSettings);
  }
}
