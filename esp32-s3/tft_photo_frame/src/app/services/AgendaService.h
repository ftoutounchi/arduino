#pragma once

#include <Arduino.h>

#include "app/ports/AgendaProvider.h"

class AgendaService {
 public:
  static constexpr uint8_t kMaxItems = 4;

  struct Result {
    bool success;
    String statusText;
    String updatedText;
    String lines[kMaxItems];
    uint8_t lineCount;
  };

  explicit AgendaService(IAgendaProvider& provider);

  void fetch(Result* outResult) const;

 private:
  IAgendaProvider& provider_;

  static void toStartShort(const String& isoDateTime, String* outShort);
};
