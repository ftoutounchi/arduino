#include "app/services/AutoViewState.h"

AutoViewState::AutoViewState()
    : photosSinceLastInfoPage_(0), hasShownAnyPhoto_(false), nextPageEntryFromAutoCycle_(false) {}

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

void AutoViewState::setNextPageEntryFromAutoCycle(bool enabled) {
  nextPageEntryFromAutoCycle_ = enabled;
}

bool AutoViewState::consumeNextPageEntryFromAutoCycle() {
  const bool value = nextPageEntryFromAutoCycle_;
  nextPageEntryFromAutoCycle_ = false;
  return value;
}
