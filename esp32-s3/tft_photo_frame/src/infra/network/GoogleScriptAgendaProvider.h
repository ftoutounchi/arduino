#pragma once

#include <Arduino.h>

#include "app/ports/AgendaProvider.h"
#include "infra/wifi/WifiManager.h"

class GoogleScriptAgendaProvider : public IAgendaProvider {
 public:
  explicit GoogleScriptAgendaProvider(WifiManager& wifi);

  bool fetchEvents(Event* outEvents,
                   uint8_t maxEvents,
                   uint8_t* outCount,
                   String* outErrorText) override;

 private:
  WifiManager& wifi_;
};
