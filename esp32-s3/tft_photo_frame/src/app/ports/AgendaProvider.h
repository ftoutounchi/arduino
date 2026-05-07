#pragma once

#include <Arduino.h>

class IAgendaProvider {
 public:
  struct Event {
    String title;
    String startIso;
  };

  virtual ~IAgendaProvider() = default;

  virtual bool fetchEvents(Event* outEvents,
                           uint8_t maxEvents,
                           uint8_t* outCount,
                           String* outErrorText) = 0;
};
