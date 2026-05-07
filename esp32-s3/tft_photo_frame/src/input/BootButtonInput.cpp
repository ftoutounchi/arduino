#include "input/BootButtonInput.h"

#include "config/AppConfig.h"

BootButtonInput::BootButtonInput()
    : rawPressed_(false),
      stablePressed_(false),
      pressArmed_(false),
      holdTriggered_(false),
      lastChangeMs_(0),
      pressStartMs_(0),
      lastEventMs_(0) {}

void BootButtonInput::begin() {
  pinMode(Config::kPinBootButton, Config::kBootButtonActiveLow ? INPUT_PULLUP : INPUT);
  if (Config::kPinBootButtonAlt >= 0 && Config::kPinBootButtonAlt != Config::kPinBootButton) {
    pinMode(static_cast<uint8_t>(Config::kPinBootButtonAlt), Config::kBootButtonActiveLow ? INPUT_PULLUP : INPUT);
  }

  rawPressed_ = readPressed();
  stablePressed_ = rawPressed_;
  pressArmed_ = false;
  holdTriggered_ = false;
  lastChangeMs_ = millis();
  pressStartMs_ = 0;
  lastEventMs_ = 0;
}

bool BootButtonInput::poll(PageEvent* outEvent) {
  if (outEvent == nullptr) {
    return false;
  }

  bool primaryPressed = false;
  bool altPressed = false;
  const bool pressed = readPressed(&primaryPressed, &altPressed);
  const uint32_t now = millis();

  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    lastChangeMs_ = now;
  }

  if ((now - lastChangeMs_) >= Config::kBootButtonDebounceMs && stablePressed_ != rawPressed_) {
    stablePressed_ = rawPressed_;

    if (stablePressed_) {
      pressArmed_ = true;
      holdTriggered_ = false;
      pressStartMs_ = now;
      return false;
    }

    // Stable release.
    if (holdTriggered_) {
      holdTriggered_ = false;
      pressArmed_ = false;
      return false;
    }

    if (!pressArmed_ || (now - lastEventMs_) < Config::kBootButtonDebounceMs) {
      pressArmed_ = false;
      return false;
    }

    pressArmed_ = false;
    lastEventMs_ = now;
    outEvent->type = PageEvent::Type::kBootShortPress;
    return true;
  }

  if (stablePressed_ && pressArmed_ && !holdTriggered_ &&
      (now - pressStartMs_) >= Config::kBootButtonHoldMs) {
    holdTriggered_ = true;
    lastEventMs_ = now;
    outEvent->type = PageEvent::Type::kBootLongPress;
    return true;
  }

  (void)primaryPressed;
  (void)altPressed;
  return false;
}

bool BootButtonInput::readPressed(bool* outPrimaryPressed, bool* outAltPressed) const {
  const bool primaryPressed =
      Config::kBootButtonActiveLow ? (digitalRead(Config::kPinBootButton) == LOW)
                                   : (digitalRead(Config::kPinBootButton) == HIGH);
  bool altPressed = false;
  if (Config::kPinBootButtonAlt >= 0 && Config::kPinBootButtonAlt != Config::kPinBootButton) {
    altPressed = Config::kBootButtonActiveLow
                     ? (digitalRead(static_cast<uint8_t>(Config::kPinBootButtonAlt)) == LOW)
                     : (digitalRead(static_cast<uint8_t>(Config::kPinBootButtonAlt)) == HIGH);
  }

  if (outPrimaryPressed != nullptr) {
    *outPrimaryPressed = primaryPressed;
  }
  if (outAltPressed != nullptr) {
    *outAltPressed = altPressed;
  }

  return primaryPressed || altPressed;
}
