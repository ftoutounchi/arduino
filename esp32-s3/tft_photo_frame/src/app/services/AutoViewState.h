#pragma once

#include <Arduino.h>

class AutoViewState {
 public:
  AutoViewState();

  void markPhotoDisplayed();
  void resetPhotoCounter();
  uint16_t photosSinceLastInfoPage() const;
  bool hasShownAnyPhoto() const;

  void setDashboardEntryFromAutoCycle(bool enabled);
  bool dashboardEntryFromAutoCycle() const;

 private:
  uint16_t photosSinceLastInfoPage_;
  bool hasShownAnyPhoto_;
  bool dashboardEntryFromAutoCycle_;
};
