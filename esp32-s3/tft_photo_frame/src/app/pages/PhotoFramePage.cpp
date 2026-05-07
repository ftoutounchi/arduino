#include "app/pages/PhotoFramePage.h"

#include <Adafruit_ST77xx.h>

#include "config/AppConfig.h"

PhotoFramePage::PhotoFramePage(DisplayRenderer& display,
                               PhotoDownloadService& photoDownloads,
                               IPageNavigator& navigator,
                               AutoViewState& autoViewState)
    : display_(display),
      photoDownloads_(photoDownloads),
      navigator_(navigator),
      autoViewState_(autoViewState) {}

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
    autoViewState_.setDashboardEntryFromAutoCycle(true);
    navigator_.requestSwitch(Id::kDashboard);
    return;
  }

  if (photoDownloads_.shouldStartDownload()) {
    photoDownloads_.startBackgroundDownload();
  }
}

void PhotoFramePage::render() {}

void PhotoFramePage::handleEvent(const PageEvent& event) {
  if (event.type == PageEvent::Type::kBootShortPress) {
    autoViewState_.setDashboardEntryFromAutoCycle(false);
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
