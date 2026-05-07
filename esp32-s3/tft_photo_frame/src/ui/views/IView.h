#pragma once

#include <Arduino.h>

#include "app/navigation/PageEvent.h"

enum class ViewAction : uint8_t {
  kNone = 0,
  kExitToPhotoFrame,
  kCloseView,
};

class IView {
 public:
  virtual ~IView() = default;

  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void update(uint32_t nowMs) = 0;
  virtual void render() = 0;
  virtual ViewAction handleEvent(const PageEvent& event) = 0;
};
