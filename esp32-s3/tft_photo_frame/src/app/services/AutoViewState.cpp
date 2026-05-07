#include "app/services/AutoViewState.h"

AutoViewState::AutoViewState()
    : photosSinceLastInfoPage_(0), hasShownAnyPhoto_(false), dashboardEntryFromAutoCycle_(false) {}

void AutoViewState::markPhotoDisplayed() {
  hasShownAnyPhoto_ = true;
  if (photosSinceLastInfoPage_ < UINT16_MAX) {
    ++photosSinceLastInfoPage_;
  }
}

void AutoViewState::resetPhotoCounter() {
  photosSinceLastInfoPage_ = 0;
}

uint16_t AutoViewState::photosSinceLastInfoPage() const {
  return photosSinceLastInfoPage_;
}

bool AutoViewState::hasShownAnyPhoto() const {
  return hasShownAnyPhoto_;
}

void AutoViewState::setDashboardEntryFromAutoCycle(bool enabled) {
  dashboardEntryFromAutoCycle_ = enabled;
}

bool AutoViewState::dashboardEntryFromAutoCycle() const {
  return dashboardEntryFromAutoCycle_;
}
