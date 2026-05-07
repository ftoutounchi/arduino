#pragma once

#include <Arduino.h>

struct PageEvent {
  enum class Type : uint8_t {
    kBootShortPress,
    kBootLongPress,
  };

  Type type;
};
