#include "app/pages/DashboardPage.h"

#include "config/AppConfig.h"

DashboardPage::DashboardPage(LvglHost& host,
                             WeatherService& weatherService,
                             IPageNavigator& navigator,
                             AutoViewState& autoViewState)
    : view_(host, weatherService),
      navigator_(navigator),
      autoViewState_(autoViewState),
      enteredAtMs_(0),
      enteredFromAutoCycle_(false) {}

void DashboardPage::onEnter() {
  view_.onEnter();

  enteredFromAutoCycle_ = autoViewState_.dashboardEntryFromAutoCycle();
  autoViewState_.setDashboardEntryFromAutoCycle(false);
  autoViewState_.resetPhotoCounter();
  enteredAtMs_ = millis();
}

void DashboardPage::onExit() {
  view_.onExit();
  enteredAtMs_ = 0;
  enteredFromAutoCycle_ = false;
}

void DashboardPage::update(uint32_t nowMs) {
  view_.update(nowMs);

  const Config::AutoViewSettings& settings = Config::gAutoViewSettings;
  if (enteredAtMs_ == 0) {
    return;
  }

  uint32_t dwellMs = 0;
  if (enteredFromAutoCycle_) {
    if (!settings.infoPageAutoCycleEnabled) {
      return;
    }
    dwellMs = settings.infoPageAutoCycleDurationMs;
  } else {
    if (!settings.infoPageAutoTimeoutEnabled) {
      return;
    }
    dwellMs = settings.infoPageAutoTimeoutMs;
  }

  if (dwellMs > 0 && (nowMs - enteredAtMs_) >= dwellMs) {
    navigator_.requestSwitch(Id::kPhotoFrame);
  }
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
