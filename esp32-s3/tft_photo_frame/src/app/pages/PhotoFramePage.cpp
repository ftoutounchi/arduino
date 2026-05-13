#include "app/pages/PhotoFramePage.h"

#include <Adafruit_ST77xx.h>

#include "config/AppConfig.h"

namespace {
struct AutoCyclePageOption {
  uint8_t bit;
  IPage::Id id;
};

constexpr AutoCyclePageOption kAutoCyclePageOptions[] = {
    {Config::kAutoCyclePageDashboard, IPage::Id::kDashboard},
    {Config::kAutoCyclePageCalendar, IPage::Id::kCalendar},
    {Config::kAutoCyclePageAgenda, IPage::Id::kAgenda},
    {Config::kAutoCyclePageAlarm, IPage::Id::kAlarm},
};
constexpr uint8_t kAutoCycleOptionCount =
    static_cast<uint8_t>(sizeof(kAutoCyclePageOptions) / sizeof(kAutoCyclePageOptions[0]));
}  // namespace

PhotoFramePage::PhotoFramePage(DisplayRenderer& display,
                               PhotoDownloadService& photoDownloads,
                               IPageNavigator& navigator,
                               AutoViewState& autoViewState)
    : display_(display),
      photoDownloads_(photoDownloads),
      navigator_(navigator),
      autoViewState_(autoViewState),
      autoCycleCursor_(0) {}

void PhotoFramePage::onEnter() {
  display_.fillScreen(ST77XX_BLACK);
  display_.showStatus("Photo frame", "Loading next image...");
}

void PhotoFramePage::onExit() {
  display_.fillScreen(ST77XX_BLACK);
}

void PhotoFramePage::update(uint32_t nowMs) {
  (void)nowMs;

  String failureText;
  if (photoDownloads_.takeFailureMessage(&failureText)) {
    showDownloadFailure(failureText.c_str());
  }

  uint8_t* data = nullptr;
  size_t len = 0;
  String exifDateTime;
  if (photoDownloads_.takeReadyImage(&data, &len, &exifDateTime)) {
    if (showPhoto(data, len, exifDateTime)) {
      autoViewState_.markPhotoDisplayed();
      Serial.println("PhotoFramePage: image swapped on screen");
    } else {
      Serial.println("PhotoFramePage: image decode failed");
    }
    free(data);
  }

  const Config::AutoViewSettings& settings = Config::gAutoViewSettings;
  if (settings.infoPageAutoCycleEnabled && settings.infoPageAutoCyclePhotoCount > 0 &&
      autoViewState_.hasShownAnyPhoto() &&
      autoViewState_.photosSinceLastInfoPage() >= settings.infoPageAutoCyclePhotoCount) {
    autoViewState_.resetPhotoCounter();
    autoViewState_.setNextPageEntryFromAutoCycle(true);
    navigator_.requestSwitch(nextAutoCycleTarget(settings.infoPageAutoCyclePagesMask));
    return;
  }

  if (photoDownloads_.shouldStartDownload()) {
    photoDownloads_.startBackgroundDownload();
  }
}

void PhotoFramePage::render() {}

void PhotoFramePage::handleEvent(const PageEvent& event) {
  if (event.type == PageEvent::Type::kBootShortPress) {
    autoViewState_.resetPhotoCounter();
    autoViewState_.setNextPageEntryFromAutoCycle(false);
    navigator_.requestSwitch(Id::kDashboard);
    return;
  }

  if (event.type == PageEvent::Type::kBootLongPress) {
    navigator_.requestPush(Id::kSettings);
  }
}

bool PhotoFramePage::showPhoto(const uint8_t* data, size_t len, const String& exifDateTime) {
  return display_.renderJpeg(data, len, exifDateTime.c_str());
}

void PhotoFramePage::showDownloadFailure(const char* text) {
  display_.showStatus("Download failed", text);
}

void PhotoFramePage::showConnecting(const char* ssid) {
  display_.showStatus("Connecting WiFi", ssid);
}

void PhotoFramePage::showWifiFailed() {
  display_.showStatus("WiFi failed", "Check SSID/password");
}

void PhotoFramePage::showInitialDownloadStatus() {
  display_.showStatus("Downloading", "first image...");
}

IPage::Id PhotoFramePage::nextAutoCycleTarget(uint8_t pagesMask) {
  uint8_t filteredMask = pagesMask & Config::kAutoCyclePageMaskAll;
  if (filteredMask == 0) {
    filteredMask = Config::kDefaultInfoPageAutoCyclePagesMask;
  }

  for (uint8_t i = 0; i < kAutoCycleOptionCount; ++i) {
    const uint8_t idx = static_cast<uint8_t>((autoCycleCursor_ + i) % kAutoCycleOptionCount);
    if ((filteredMask & kAutoCyclePageOptions[idx].bit) != 0) {
      autoCycleCursor_ = static_cast<uint8_t>((idx + 1U) % kAutoCycleOptionCount);
      return kAutoCyclePageOptions[idx].id;
    }
  }

  autoCycleCursor_ = 1;
  return IPage::Id::kDashboard;
}
