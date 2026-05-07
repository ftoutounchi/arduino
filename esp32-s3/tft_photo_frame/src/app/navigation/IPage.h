#pragma once

#include <Arduino.h>

#include "app/navigation/PageEvent.h"

class IPage {
 public:
  enum class Id : uint8_t {
    kPhotoFrame = 0,
    kDashboard,
    kCalendar,
    kAgenda,
    kAlarm,
    kSettings,
    kCount
  };

  virtual ~IPage() = default;

  virtual Id id() const = 0;
  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void update(uint32_t nowMs) = 0;
  virtual void render() = 0;
  virtual void handleEvent(const PageEvent& event) = 0;
};
