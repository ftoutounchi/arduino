#include "app/services/AgendaService.h"

#include <time.h>

AgendaService::AgendaService(IAgendaProvider& provider) : provider_(provider) {}

void AgendaService::fetch(Result* outResult) const {
  if (outResult == nullptr) {
    return;
  }

  outResult->success = false;
  outResult->statusText = "Agenda Error";
  outResult->updatedText = "Updated: --:--";
  outResult->lineCount = 0;
  for (uint8_t i = 0; i < kMaxItems; ++i) {
    outResult->lines[i] = "--";
  }

  IAgendaProvider::Event events[kMaxItems];
  uint8_t count = 0;
  String errorText;
  if (!provider_.fetchEvents(events, kMaxItems, &count, &errorText)) {
    outResult->statusText = errorText.isEmpty() ? "Agenda Error" : errorText;
    return;
  }

  if (count == 0) {
    outResult->success = true;
    outResult->statusText = "Google Calendar";
    outResult->lines[0] = "No upcoming events";
    outResult->lineCount = 1;
  } else {
    outResult->success = true;
    outResult->statusText = "Google Calendar";
    outResult->lineCount = count;

    for (uint8_t i = 0; i < count; ++i) {
      String startShort;
      toStartShort(events[i].startIso, &startShort);
      if (startShort.length() > 0) {
        outResult->lines[i] = String(i + 1) + ". " + events[i].title + "\n" + startShort;
      } else {
        outResult->lines[i] = String(i + 1) + ". " + events[i].title;
      }
    }
  }

  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 20)) {
    char updated[24];
    snprintf(updated, sizeof(updated), "Updated: %02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    outResult->updatedText = updated;
  } else {
    outResult->updatedText = "Updated: now";
  }
}

void AgendaService::toStartShort(const String& isoDateTime, String* outShort) {
  if (outShort == nullptr) {
    return;
  }

  *outShort = "";
  if (isoDateTime.length() == 0) {
    return;
  }

  for (size_t i = 0; i < static_cast<size_t>(isoDateTime.length()); ++i) {
    const char ch = isoDateTime[i];
    if (ch == 'T') {
      outShort->concat(' ');
    } else if (ch == 'Z' || ch == '+') {
      break;
    } else {
      outShort->concat(ch);
    }
  }
}
