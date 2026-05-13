#include "photo_frame/PhotoFrameApp.h"

#include <esp_system.h>
#include <time.h>

#include "config/AppConfig.h"

PhotoFrameApp::PhotoFrameApp()
    : display_(),
      wifi_(),
      downloader_(wifi_),
      photoDownloads_(downloader_),
      agendaProvider_(wifi_),
      weatherProvider_(wifi_),
      agendaService_(agendaProvider_),
      alarmScheduler_(agendaProvider_),
      weatherService_(weatherProvider_),
      autoViewState_(),
      lvglHost_(display_),
      pageManager_(),
      photoFramePage_(display_, photoDownloads_, pageManager_, autoViewState_),
      dashboardPage_(lvglHost_, weatherService_, pageManager_),
      calendarPage_(lvglHost_, pageManager_),
      agendaPage_(lvglHost_, agendaService_, pageManager_),
      alarmPage_(lvglHost_, alarmScheduler_, pageManager_),
      settingsPage_(lvglHost_, display_, pageManager_),
      bootButton_(),
      configWeb_(),
      configReloadPending_(false),
      globalTimeoutPageTracked_(false),
      globalTimeoutPageId_(IPage::Id::kPhotoFrame),
      globalTimeoutEntryFromAutoCycle_(false),
      globalTimeoutEnteredAtMs_(0) {}

void PhotoFrameApp::begin() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("Reset reason: %d\n", static_cast<int>(esp_reset_reason()));

  randomSeed(static_cast<uint32_t>(esp_random()) ^ micros());

  display_.begin();
  bootButton_.begin();

  if (!photoDownloads_.begin()) {
    display_.showStatus("Startup failed", "Download service");
    return;
  }

  registerPages();
  if (!pageManager_.start(IPage::Id::kPhotoFrame)) {
    display_.showStatus("Startup failed", "Page manager");
    return;
  }

  photoFramePage_.showConnecting(Config::kWifiSsid);
  if (!wifi_.connect()) {
    photoFramePage_.showWifiFailed();
    return;
  }

  Config::loadAutoViewSettings();
  display_.applyDisplayTransform();
  alarmScheduler_.begin();

  configWeb_.setConfigChangedCallback(&PhotoFrameApp::onWebConfigChanged, this);
  configWeb_.begin();

  lvglHost_.begin();
  configTzTime(Config::kTimeZone, Config::kNtpServer1, Config::kNtpServer2);
  photoDownloads_.resetDownloader();

  photoFramePage_.showInitialDownloadStatus();
  photoDownloads_.startBackgroundDownload();
}

void PhotoFrameApp::loop() {
  configWeb_.loop();
  processConfigReload();

  PageEvent event = {PageEvent::Type::kBootShortPress};
  if (bootButton_.poll(&event)) {
    pageManager_.handleEvent(event);
  }

  const uint32_t now = millis();
  alarmScheduler_.update(now);
  const IPage* activePage = pageManager_.currentPage();
  if (alarmScheduler_.hasActiveAlert() && (activePage == nullptr || activePage->id() != IPage::Id::kAlarm)) {
    pageManager_.requestSwitch(IPage::Id::kAlarm);
  }
  pageManager_.update(now);
  applyGlobalPageAutoTimeout(now);
  pageManager_.render();

  // Poll twice to reduce blind spots for short taps.
  if (bootButton_.poll(&event)) {
    pageManager_.handleEvent(event);
  }

  delay(1);
}

void PhotoFrameApp::onWebConfigChanged(void* ctx) {
  if (ctx == nullptr) {
    return;
  }
  static_cast<PhotoFrameApp*>(ctx)->requestConfigReload();
}

void PhotoFrameApp::registerPages() {
  pageManager_.registerPage(photoFramePage_);
  pageManager_.registerPage(dashboardPage_);
  pageManager_.registerPage(calendarPage_);
  pageManager_.registerPage(agendaPage_);
  pageManager_.registerPage(alarmPage_);
  pageManager_.registerPage(settingsPage_);
}

void PhotoFrameApp::requestConfigReload() {
  configReloadPending_ = true;
}

void PhotoFrameApp::processConfigReload() {
  if (!configReloadPending_) {
    return;
  }

  if (photoDownloads_.isDownloadInProgress()) {
    return;
  }

  Config::loadAutoViewSettings();
  display_.applyDisplayTransform();
  alarmScheduler_.begin();
  photoDownloads_.resetDownloader();
  configReloadPending_ = false;
  Serial.println("WebConfig: runtime config reloaded");
}

void PhotoFrameApp::applyGlobalPageAutoTimeout(uint32_t nowMs) {
  const IPage* activePage = pageManager_.currentPage();
  if (activePage == nullptr) {
    globalTimeoutPageTracked_ = false;
    globalTimeoutEntryFromAutoCycle_ = false;
    globalTimeoutEnteredAtMs_ = 0;
    return;
  }

  const IPage::Id pageId = activePage->id();
  if (!globalTimeoutPageTracked_ || globalTimeoutPageId_ != pageId) {
    globalTimeoutPageTracked_ = true;
    globalTimeoutPageId_ = pageId;
    globalTimeoutEnteredAtMs_ = nowMs;
    globalTimeoutEntryFromAutoCycle_ = autoViewState_.consumeNextPageEntryFromAutoCycle();
    if (pageId != IPage::Id::kPhotoFrame) {
      autoViewState_.resetPhotoCounter();
    }
    return;
  }

  if (pageId == IPage::Id::kPhotoFrame) {
    return;
  }

  uint32_t dwellMs = 0;
  if (globalTimeoutEntryFromAutoCycle_) {
    dwellMs = Config::gAutoViewSettings.infoPageAutoCycleDurationMs;
  } else {
    if (!Config::gAutoViewSettings.infoPageAutoTimeoutEnabled) {
      return;
    }
    dwellMs = Config::gAutoViewSettings.infoPageAutoTimeoutMs;
  }

  // Keep alert page visible while an alert is active.
  if (pageId == IPage::Id::kAlarm && alarmScheduler_.hasActiveAlert()) {
    return;
  }

  if (dwellMs > 0 && (nowMs - globalTimeoutEnteredAtMs_) >= dwellMs) {
    globalTimeoutPageTracked_ = false;
    globalTimeoutEntryFromAutoCycle_ = false;
    pageManager_.requestSwitch(IPage::Id::kPhotoFrame);
  }
}
