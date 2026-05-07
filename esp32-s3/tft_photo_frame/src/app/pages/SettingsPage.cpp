#include "app/pages/SettingsPage.h"

SettingsPage::SettingsPage(LvglHost& host, DisplayRenderer& display, IPageNavigator& navigator)
    : view_(host, display), navigator_(navigator) {}

void SettingsPage::onEnter() {
  view_.onEnter();
}

void SettingsPage::onExit() {
  view_.onExit();
}

void SettingsPage::update(uint32_t nowMs) {
  view_.update(nowMs);
}

void SettingsPage::render() {
  view_.render();
}

void SettingsPage::handleEvent(const PageEvent& event) {
  const ViewAction action = view_.handleEvent(event);
  if (action == ViewAction::kCloseView) {
    navigator_.requestPop();
  }
}
