#pragma once

#include <Arduino.h>

#include "app/ports/AgendaProvider.h"
#include "config/AppConfig.h"

class AlarmScheduler {
 public:
  explicit AlarmScheduler(IAgendaProvider& agendaProvider);

  void begin();
  void update(uint32_t nowMs);

  bool hasActiveAlert() const;
  bool activeAlertFromEvent() const;
  bool activeAlertSoundEnabled() const;
  const char* activeAlertLabel() const;
  uint8_t activeAlertHour() const;
  uint8_t activeAlertMinute() const;

  bool acknowledgeActiveAlert();

 private:
  struct AlertState {
    bool fromEvent;
    bool soundEnabled;
    uint8_t hour;
    uint8_t minute;
    char label[Config::kMaxAlarmLabelLength];
  };

  struct CachedAgendaEvent {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint32_t minuteEpoch;
    char title[Config::kMaxAlarmLabelLength];
  };

  static constexpr uint8_t kMaxAgendaEvents = 8;
  static constexpr uint16_t kBuzzerFrequencyHz = 2400;
  static constexpr uint16_t kBuzzerOnMs = 180;
  static constexpr uint16_t kBuzzerOffMs = 820;

  IAgendaProvider& agendaProvider_;

  uint32_t lastCheckedMinute_;
  uint32_t lastAgendaUpdateMs_;
  uint32_t lastAgendaAttemptMs_;
  uint32_t lastTriggeredMinuteByAlarm_[Config::kMaxAlarms];
  uint32_t lastTriggeredMinuteByEvent_[kMaxAgendaEvents];

  CachedAgendaEvent cachedEvents_[kMaxAgendaEvents];
  uint8_t cachedEventCount_;

  bool alertActive_;
  AlertState activeAlert_;
  bool pendingAlertValid_;
  AlertState pendingAlert_;

  bool buzzerArmed_;
  bool buzzerToneOn_;
  uint32_t buzzerPhaseStartMs_;

  bool refreshAgendaCache();
  void checkMinuteTick(uint32_t nowMs);
  void updateBuzzer(uint32_t nowMs);

  static bool parseIsoEventMinute(const String& iso,
                                  uint16_t* outYear,
                                  uint8_t* outMonth,
                                  uint8_t* outDay,
                                  uint8_t* outHour,
                                  uint8_t* outMinute);
  static bool alarmMatchesLocalMinute(const Config::AlarmEntry& alarm, const tm& localTime);
  static bool isAlarmOneTime(const Config::AlarmEntry& alarm);
  static void copyCString(char* dst, size_t dstSize, const char* src);

  void queueAlert(const AlertState& alert, uint32_t nowMs);
  void activateAlert(const AlertState& alert, uint32_t nowMs);
  void stopBuzzer();
};
