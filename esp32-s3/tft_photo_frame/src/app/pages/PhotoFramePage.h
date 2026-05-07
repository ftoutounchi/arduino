#pragma once

#include "infra/display/DisplayRenderer.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"
#include "app/services/AutoViewState.h"
#include "app/services/PhotoDownloadService.h"

class PhotoFramePage : public IPage {
 public:
  PhotoFramePage(DisplayRenderer& display,
                 PhotoDownloadService& photoDownloads,
                 IPageNavigator& navigator,
                 AutoViewState& autoViewState);

  Id id() const override { return Id::kPhotoFrame; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

  bool showPhoto(const uint8_t* data, size_t len, const String& exifDateTime);
  void showDownloadFailure(const char* text);
  void showConnecting(const char* ssid);
  void showWifiFailed();
  void showInitialDownloadStatus();

 private:
  DisplayRenderer& display_;
  PhotoDownloadService& photoDownloads_;
  IPageNavigator& navigator_;
  AutoViewState& autoViewState_;
};
