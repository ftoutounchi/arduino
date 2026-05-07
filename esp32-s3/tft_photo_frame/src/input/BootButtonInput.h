#pragma once

#include <Arduino.h>

#include "app/navigation/PageEvent.h"

class BootButtonInput {
 public:
  BootButtonInput();

  void begin();
  bool poll(PageEvent* outEvent);

 private:
  bool rawPressed_;
  bool stablePressed_;
  bool pressArmed_;
  bool holdTriggered_;
  uint32_t lastChangeMs_;
  uint32_t pressStartMs_;
  uint32_t lastEventMs_;

  bool readPressed(bool* outPrimaryPressed = nullptr, bool* outAltPressed = nullptr) const;
};
