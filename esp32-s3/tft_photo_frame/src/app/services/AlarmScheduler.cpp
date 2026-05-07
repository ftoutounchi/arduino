#include "app/services/AlarmScheduler.h"

#include <string.h>
#include <time.h>

AlarmScheduler::AlarmScheduler(IAgendaProvider& agendaProvider)
    : agendaProvider_(agendaProvider),
      lastCheckedMinute_(UINT32_MAX),
      lastAgendaUpdateMs_(0),
      lastAgendaAttemptMs_(0),
      lastTriggeredMinuteByAlarm_{},
      lastTriggeredMinuteByEvent_{},
      cachedEvents_{},
      cachedEventCount_(0),
      alertActive_(false),
      activeAlert_{false, false, 0, 0, {0}},
      pendingAlertValid_(false),
      pendingAlert_{false, false, 0, 0, {0}},
      buzzerArmed_(false),
      buzzerToneOn_(false),
      buzzerPhaseStartMs_(0) {
  for (uint8_t i = 0; i < Config::kMaxAlarms; ++i) {
    lastTriggeredMinuteByAlarm_[i] = UINT32_MAX;
  }
  for (uint8_t i = 0; i < kMaxAgendaEvents; ++i) {
    lastTriggeredMinuteByEvent_[i] = UINT32_MAX;
  }
}

void AlarmScheduler::begin() {
  pinMode(Config::kPinBuzzer, OUTPUT);
  lastCheckedMinute_ = UINT32_MAX;
  lastAgendaUpdateMs_ = 0;
  lastAgendaAttemptMs_ = 0;
  for (uint8_t i = 0; i < Config::kMaxAlarms; ++i) {
    lastTriggeredMinuteByAlarm_[i] = UINT32_MAX;
  }
  for (uint8_t i = 0; i < kMaxAgendaEvents; ++i) {
    lastTriggeredMinuteByEvent_[i] = UINT32_MAX;
  }
  stopBuzzer();
  refreshAgendaCache();
}

void AlarmScheduler::update(uint32_t nowMs) {
  updateBuzzer(nowMs);
  checkMinuteTick(nowMs);
}

bool AlarmScheduler::hasActiveAlert() const {
  return alertActive_;
}

bool AlarmScheduler::activeAlertFromEvent() const {
  return alertActive_ ? activeAlert_.fromEvent : false;
}

bool AlarmScheduler::activeAlertSoundEnabled() const {
  return alertActive_ ? activeAlert_.soundEnabled : false;
}

const char* AlarmScheduler::activeAlertLabel() const {
  if (!alertActive_) {
    return "";
  }
  return activeAlert_.label;
}

uint8_t AlarmScheduler::activeAlertHour() const {
  return alertActive_ ? activeAlert_.hour : 0;
}

uint8_t AlarmScheduler::activeAlertMinute() const {
  return alertActive_ ? activeAlert_.minute : 0;
}

bool AlarmScheduler::acknowledgeActiveAlert() {
  if (!alertActive_) {
    return false;
  }

  alertActive_ = false;
  buzzerArmed_ = false;
  stopBuzzer();

  if (pendingAlertValid_) {
    activateAlert(pendingAlert_, millis());
    pendingAlertValid_ = false;
  }
  return true;
}

void AlarmScheduler::checkMinuteTick(uint32_t nowMs) {
  const time_t nowEpoch = time(nullptr);
  if (nowEpoch <= 0) {
    return;
  }

  const uint32_t minuteNow = static_cast<uint32_t>(nowEpoch / 60);
  if (minuteNow == lastCheckedMinute_) {
    return;
  }
  lastCheckedMinute_ = minuteNow;

  struct tm localTime = {};
  if (localtime_r(&nowEpoch, &localTime) == nullptr) {
    return;
  }
  if ((localTime.tm_year + 1900) < 2023) {
    return;
  }

  const uint32_t agendaInterval =
      (lastAgendaUpdateMs_ == 0) ? 0 : static_cast<uint32_t>(nowMs - lastAgendaUpdateMs_);
  const uint32_t agendaRetry =
      (lastAgendaAttemptMs_ == 0) ? 0 : static_cast<uint32_t>(nowMs - lastAgendaAttemptMs_);
  const bool dueRefresh = (lastAgendaUpdateMs_ == 0) || (agendaInterval >= Config::kAgendaRefreshIntervalMs);
  const bool retryWindowOk = (lastAgendaAttemptMs_ == 0) || (agendaRetry >= Config::kAgendaRetryIntervalMs);
  if (dueRefresh && retryWindowOk) {
    lastAgendaAttemptMs_ = nowMs;
    if (refreshAgendaCache()) {
      lastAgendaUpdateMs_ = nowMs;
    }
  }

  bool configDirty = false;

  const uint8_t alarmCount = (Config::gAlarmCount <= Config::kMaxAlarms) ? Config::gAlarmCount : Config::kMaxAlarms;
  for (uint8_t i = 0; i < alarmCount; ++i) {
    const Config::AlarmEntry& alarm = Config::gAlarms[i];
    if (!alarm.enabled) {
      continue;
    }
    if (lastTriggeredMinuteByAlarm_[i] == minuteNow) {
      continue;
    }
    if (!alarmMatchesLocalMinute(alarm, localTime)) {
      continue;
    }

    lastTriggeredMinuteByAlarm_[i] = minuteNow;

    AlertState alert = {};
    alert.fromEvent = false;
    alert.soundEnabled = alarm.soundEnabled;
    alert.hour = alarm.hour;
    alert.minute = alarm.minute;
    copyCString(alert.label, sizeof(alert.label), alarm.label);
    queueAlert(alert, nowMs);

    if (isAlarmOneTime(alarm)) {
      Config::gAlarms[i].enabled = false;
      configDirty = true;
    }
  }

  for (uint8_t i = 0; i < cachedEventCount_; ++i) {
    if (!Config::gAgendaEventAlarmEnabled) {
      break;
    }

    const CachedAgendaEvent& event = cachedEvents_[i];
    if (lastTriggeredMinuteByEvent_[i] == minuteNow) {
      continue;
    }
    if (event.minuteEpoch == 0) {
      continue;
    }

    const uint32_t leadMinutes = static_cast<uint32_t>(Config::gAgendaEventLeadMinutes);
    if (event.minuteEpoch < leadMinutes) {
      continue;
    }
    const uint32_t triggerMinute = event.minuteEpoch - leadMinutes;
    if (minuteNow != triggerMinute) {
      continue;
    }

    lastTriggeredMinuteByEvent_[i] = minuteNow;

    AlertState alert = {};
    alert.fromEvent = true;
    alert.soundEnabled = Config::gAgendaEventSoundEnabled;
    alert.hour = event.hour;
    alert.minute = event.minute;
    copyCString(alert.label, sizeof(alert.label), event.title);
    queueAlert(alert, nowMs);
  }

  if (configDirty) {
    Config::saveAutoViewSettings();
  }
}

void AlarmScheduler::updateBuzzer(uint32_t nowMs) {
  if (!alertActive_ || !activeAlert_.soundEnabled || !buzzerArmed_) {
    if (!alertActive_ || !activeAlert_.soundEnabled) {
      buzzerArmed_ = false;
    }
    stopBuzzer();
    return;
  }

  const uint32_t elapsed = static_cast<uint32_t>(nowMs - buzzerPhaseStartMs_);
  if (buzzerToneOn_) {
    if (elapsed >= kBuzzerOnMs) {
      noTone(Config::kPinBuzzer);
      buzzerToneOn_ = false;
      buzzerPhaseStartMs_ = nowMs;
    }
  } else {
    if (elapsed >= kBuzzerOffMs) {
      tone(Config::kPinBuzzer, kBuzzerFrequencyHz);
      buzzerToneOn_ = true;
      buzzerPhaseStartMs_ = nowMs;
    }
  }
}

bool AlarmScheduler::refreshAgendaCache() {
  IAgendaProvider::Event events[kMaxAgendaEvents];
  uint8_t count = 0;
  String errorText;
  if (!agendaProvider_.fetchEvents(events, kMaxAgendaEvents, &count, &errorText)) {
    return false;
  }

  for (uint8_t i = 0; i < kMaxAgendaEvents; ++i) {
    lastTriggeredMinuteByEvent_[i] = UINT32_MAX;
  }

  cachedEventCount_ = 0;
  for (uint8_t i = 0; i < count && cachedEventCount_ < kMaxAgendaEvents; ++i) {
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    if (!parseIsoEventMinute(events[i].startIso, &year, &month, &day, &hour, &minute)) {
      continue;
    }

    CachedAgendaEvent& dst = cachedEvents_[cachedEventCount_++];
    dst.year = year;
    dst.month = month;
    dst.day = day;
    dst.hour = hour;
    dst.minute = minute;
    dst.minuteEpoch = 0;
    struct tm tmEvent = {};
    tmEvent.tm_year = static_cast<int>(year) - 1900;
    tmEvent.tm_mon = static_cast<int>(month) - 1;
    tmEvent.tm_mday = static_cast<int>(day);
    tmEvent.tm_hour = static_cast<int>(hour);
    tmEvent.tm_min = static_cast<int>(minute);
    tmEvent.tm_sec = 0;
    tmEvent.tm_isdst = -1;
    const time_t eventEpoch = mktime(&tmEvent);
    if (eventEpoch > 0) {
      dst.minuteEpoch = static_cast<uint32_t>(eventEpoch / 60);
    }
    copyCString(dst.title, sizeof(dst.title), events[i].title.c_str());
    if (dst.title[0] == '\0') {
      copyCString(dst.title, sizeof(dst.title), "Event");
    }
  }

  return true;
}

bool AlarmScheduler::parseIsoEventMinute(const String& iso,
                                         uint16_t* outYear,
                                         uint8_t* outMonth,
                                         uint8_t* outDay,
                                         uint8_t* outHour,
                                         uint8_t* outMinute) {
  if (outYear == nullptr || outMonth == nullptr || outDay == nullptr || outHour == nullptr || outMinute == nullptr) {
    return false;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) != 5) {
    return false;
  }

  if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59) {
    return false;
  }

  *outYear = static_cast<uint16_t>(year);
  *outMonth = static_cast<uint8_t>(month);
  *outDay = static_cast<uint8_t>(day);
  *outHour = static_cast<uint8_t>(hour);
  *outMinute = static_cast<uint8_t>(minute);
  return true;
}

bool AlarmScheduler::alarmMatchesLocalMinute(const Config::AlarmEntry& alarm, const tm& localTime) {
  if (!alarm.enabled) {
    return false;
  }
  if (alarm.hour != static_cast<uint8_t>(localTime.tm_hour) || alarm.minute != static_cast<uint8_t>(localTime.tm_min)) {
    return false;
  }

  const uint8_t repeatMode = alarm.repeatMode;
  if (repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kDaily)) {
    return true;
  }

  if (repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kWeekdays)) {
    const uint8_t weekday = static_cast<uint8_t>(localTime.tm_wday);
    const uint8_t bit = static_cast<uint8_t>(1U << weekday);
    return (alarm.repeatDaysMask & bit) != 0;
  }

  if (repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kOneTime)) {
    const uint16_t year = static_cast<uint16_t>(localTime.tm_year + 1900);
    const uint8_t month = static_cast<uint8_t>(localTime.tm_mon + 1);
    const uint8_t day = static_cast<uint8_t>(localTime.tm_mday);
    return (alarm.oneTimeYear == year) && (alarm.oneTimeMonth == month) && (alarm.oneTimeDay == day);
  }

  return true;
}

bool AlarmScheduler::isAlarmOneTime(const Config::AlarmEntry& alarm) {
  return alarm.repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kOneTime);
}

void AlarmScheduler::copyCString(char* dst, size_t dstSize, const char* src) {
  if (dst == nullptr || dstSize == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void AlarmScheduler::queueAlert(const AlertState& alert, uint32_t nowMs) {
  if (alertActive_) {
    if (strncmp(activeAlert_.label, alert.label, sizeof(activeAlert_.label)) == 0 && activeAlert_.hour == alert.hour &&
        activeAlert_.minute == alert.minute && activeAlert_.fromEvent == alert.fromEvent) {
      return;
    }
    if (!pendingAlertValid_) {
      pendingAlert_ = alert;
      pendingAlertValid_ = true;
    }
    return;
  }

  activateAlert(alert, nowMs);
}

void AlarmScheduler::activateAlert(const AlertState& alert, uint32_t nowMs) {
  activeAlert_ = alert;
  alertActive_ = true;

  buzzerArmed_ = alert.soundEnabled;
  buzzerToneOn_ = false;
  buzzerPhaseStartMs_ = nowMs;

  stopBuzzer();
  if (buzzerArmed_) {
    tone(Config::kPinBuzzer, kBuzzerFrequencyHz);
    buzzerToneOn_ = true;
    buzzerPhaseStartMs_ = nowMs;
  }
}

void AlarmScheduler::stopBuzzer() {
  noTone(Config::kPinBuzzer);
  buzzerToneOn_ = false;
}
