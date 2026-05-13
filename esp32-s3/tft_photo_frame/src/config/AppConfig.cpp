#include "config/AppConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstring>

namespace Config {
namespace {
void copyCString(char* dst, size_t dstSize, const char* src) {
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

bool isPlausibleOneTimeDate(uint16_t year, uint8_t month, uint8_t day) {
  if (year < 2020 || year > 2099) {
    return false;
  }
  if (month < 1 || month > 12) {
    return false;
  }
  if (day < 1 || day > 31) {
    return false;
  }
  return true;
}
}  // namespace

const char kWifiSsid[] = "Paradise";
const char kWifiPassword[] = "Arezoo&Farzad7";
const char kPhotoUrlsJsonPath[] = "/photo_urls.json";
const char kGithubAuthJsonPath[] = "/github_auth.json";
const char kSettingsJsonPath[] = "/settings.json";
const char kTimeZone[] = "CET-1CEST,M3.5.0,M10.5.0/3";
const char kNtpServer1[] = "pool.ntp.org";
const char kNtpServer2[] = "time.nist.gov";
const char kDefaultWeatherLocationLabel[] = "Hamburg 22177";
const char kDefaultAgendaScriptUrl[] =
    "https://script.google.com/macros/s/AKfycbwWdfR1xJWhKOc_bYqlSg7X01wQXHB00rgBo99R6Viw60zoKW0fYhdIs63E8koxScLV/exec";
char gWeatherLocationLabel[kMaxWeatherLabelLength] = {0};
double gWeatherLatitude = kDefaultWeatherLatitude;
double gWeatherLongitude = kDefaultWeatherLongitude;
AutoViewSettings gAutoViewSettings = {
    kDefaultInfoPageAutoTimeoutEnabled,
    kDefaultInfoPageAutoTimeoutMs,
    kDefaultInfoPageAutoCycleEnabled,
    kDefaultInfoPageAutoCyclePagesMask,
    kDefaultInfoPageAutoCyclePhotoCount,
    kDefaultInfoPageAutoCycleDurationMs,
    kDefaultPhotoRefreshIntervalMs,
    kDefaultPhotoFillMode};
char gAgendaScriptUrl[kMaxAgendaUrlLength] = {0};
AlarmEntry gAlarms[kMaxAlarms] = {};
uint8_t gAlarmCount = 0;
bool gAgendaEventAlarmEnabled = true;
bool gAgendaEventSoundEnabled = false;
uint16_t gAgendaEventLeadMinutes = 0;
uint8_t gDisplayTransform = kDefaultDisplayTransform;

void resetAutoViewSettingsToDefaults() {
  gAutoViewSettings.infoPageAutoTimeoutEnabled = kDefaultInfoPageAutoTimeoutEnabled;
  gAutoViewSettings.infoPageAutoTimeoutMs = kDefaultInfoPageAutoTimeoutMs;
  gAutoViewSettings.infoPageAutoCycleEnabled = kDefaultInfoPageAutoCycleEnabled;
  gAutoViewSettings.infoPageAutoCyclePagesMask = kDefaultInfoPageAutoCyclePagesMask;
  gAutoViewSettings.infoPageAutoCyclePhotoCount = kDefaultInfoPageAutoCyclePhotoCount;
  gAutoViewSettings.infoPageAutoCycleDurationMs = kDefaultInfoPageAutoCycleDurationMs;
  gAutoViewSettings.photoRefreshIntervalMs = kDefaultPhotoRefreshIntervalMs;
  gAutoViewSettings.photoFillMode = kDefaultPhotoFillMode;
  copyCString(gWeatherLocationLabel, sizeof(gWeatherLocationLabel), kDefaultWeatherLocationLabel);
  gWeatherLatitude = kDefaultWeatherLatitude;
  gWeatherLongitude = kDefaultWeatherLongitude;

  copyCString(gAgendaScriptUrl, sizeof(gAgendaScriptUrl), kDefaultAgendaScriptUrl);
  gDisplayTransform = kDefaultDisplayTransform;
  gAlarmCount = 1;
  for (uint8_t i = 0; i < kMaxAlarms; ++i) {
    gAlarms[i].enabled = false;
    gAlarms[i].soundEnabled = false;
    gAlarms[i].hour = 7;
    gAlarms[i].minute = 0;
    gAlarms[i].repeatMode = static_cast<uint8_t>(AlarmRepeatMode::kDaily);
    gAlarms[i].repeatDaysMask = kWeekdayMaskAll;
    gAlarms[i].oneTimeYear = 0;
    gAlarms[i].oneTimeMonth = 0;
    gAlarms[i].oneTimeDay = 0;
    gAlarms[i].label[0] = '\0';
  }
  copyCString(gAlarms[0].label, sizeof(gAlarms[0].label), "Alarm 1");
  gAgendaEventAlarmEnabled = true;
  gAgendaEventSoundEnabled = false;
  gAgendaEventLeadMinutes = 0;
}

static void normalizeAutoViewSettings() {
  if (gAutoViewSettings.infoPageAutoTimeoutMs < 1000) {
    gAutoViewSettings.infoPageAutoTimeoutMs = 1000;
  } else if (gAutoViewSettings.infoPageAutoTimeoutMs > 10UL * 60UL * 1000UL) {
    gAutoViewSettings.infoPageAutoTimeoutMs = 10UL * 60UL * 1000UL;
  }

  if (gAutoViewSettings.infoPageAutoCyclePhotoCount < 1) {
    gAutoViewSettings.infoPageAutoCyclePhotoCount = 1;
  } else if (gAutoViewSettings.infoPageAutoCyclePhotoCount > 200) {
    gAutoViewSettings.infoPageAutoCyclePhotoCount = 200;
  }

  gAutoViewSettings.infoPageAutoCyclePagesMask &= kAutoCyclePageMaskAll;
  if (gAutoViewSettings.infoPageAutoCyclePagesMask == 0) {
    gAutoViewSettings.infoPageAutoCyclePagesMask = kDefaultInfoPageAutoCyclePagesMask;
  }

  if (gAutoViewSettings.infoPageAutoCycleDurationMs < 1000) {
    gAutoViewSettings.infoPageAutoCycleDurationMs = 1000;
  } else if (gAutoViewSettings.infoPageAutoCycleDurationMs > 10UL * 60UL * 1000UL) {
    gAutoViewSettings.infoPageAutoCycleDurationMs = 10UL * 60UL * 1000UL;
  }

  if (gAutoViewSettings.photoRefreshIntervalMs < 1000) {
    gAutoViewSettings.photoRefreshIntervalMs = 1000;
  } else if (gAutoViewSettings.photoRefreshIntervalMs > 10UL * 60UL * 1000UL) {
    gAutoViewSettings.photoRefreshIntervalMs = 10UL * 60UL * 1000UL;
  }

  if (gAgendaScriptUrl[0] == '\0') {
    copyCString(gAgendaScriptUrl, sizeof(gAgendaScriptUrl), kDefaultAgendaScriptUrl);
  }
  if (gWeatherLocationLabel[0] == '\0') {
    copyCString(gWeatherLocationLabel, sizeof(gWeatherLocationLabel), kDefaultWeatherLocationLabel);
  }
  if (gWeatherLatitude < -90.0) {
    gWeatherLatitude = -90.0;
  } else if (gWeatherLatitude > 90.0) {
    gWeatherLatitude = 90.0;
  }
  if (gWeatherLongitude < -180.0) {
    gWeatherLongitude = -180.0;
  } else if (gWeatherLongitude > 180.0) {
    gWeatherLongitude = 180.0;
  }
  if (gDisplayTransform > kDisplayTransformMax) {
    gDisplayTransform = kDefaultDisplayTransform;
  }
  if (gAlarmCount > kMaxAlarms) {
    gAlarmCount = kMaxAlarms;
  }
  for (uint8_t i = 0; i < gAlarmCount; ++i) {
    if (gAlarms[i].hour > 23) {
      gAlarms[i].hour = 23;
    }
    if (gAlarms[i].minute > 59) {
      gAlarms[i].minute = 59;
    }
    if (gAlarms[i].label[0] == '\0') {
      char fallback[16];
      snprintf(fallback, sizeof(fallback), "Alarm %u", static_cast<unsigned>(i + 1));
      copyCString(gAlarms[i].label, sizeof(gAlarms[i].label), fallback);
    }
    if (gAlarms[i].repeatMode > static_cast<uint8_t>(AlarmRepeatMode::kOneTime)) {
      gAlarms[i].repeatMode = static_cast<uint8_t>(AlarmRepeatMode::kDaily);
    }
    gAlarms[i].repeatDaysMask &= kWeekdayMaskAll;
    if (gAlarms[i].repeatMode == static_cast<uint8_t>(AlarmRepeatMode::kWeekdays) &&
        gAlarms[i].repeatDaysMask == 0) {
      gAlarms[i].repeatDaysMask = kWeekdayMaskMonToFri;
    }
    if (gAlarms[i].repeatMode == static_cast<uint8_t>(AlarmRepeatMode::kOneTime) &&
        !isPlausibleOneTimeDate(gAlarms[i].oneTimeYear, gAlarms[i].oneTimeMonth, gAlarms[i].oneTimeDay)) {
      gAlarms[i].repeatMode = static_cast<uint8_t>(AlarmRepeatMode::kDaily);
      gAlarms[i].oneTimeYear = 0;
      gAlarms[i].oneTimeMonth = 0;
      gAlarms[i].oneTimeDay = 0;
    }
  }
  if (gAgendaEventLeadMinutes > 7U * 24U * 60U) {
    gAgendaEventLeadMinutes = 7U * 24U * 60U;
  }
}

bool loadAutoViewSettings() {
  resetAutoViewSettingsToDefaults();

  if (!LittleFS.begin(false)) {
    Serial.println("Settings: LittleFS mount failed; using defaults");
    return false;
  }

  File file = LittleFS.open(kSettingsJsonPath, "r");
  if (!file) {
    Serial.println("Settings: settings.json not found; using defaults");
    return false;
  }

  DynamicJsonDocument doc(4096);
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.printf("Settings: parse error (%s), using defaults\n", err.c_str());
    return false;
  }

  gAutoViewSettings.infoPageAutoTimeoutEnabled =
      doc["auto_timeout_enabled"] | kDefaultInfoPageAutoTimeoutEnabled;
  gAutoViewSettings.infoPageAutoTimeoutMs = doc["auto_timeout_ms"] | kDefaultInfoPageAutoTimeoutMs;
  gAutoViewSettings.infoPageAutoCycleEnabled = doc["auto_cycle_enabled"] | kDefaultInfoPageAutoCycleEnabled;
  gAutoViewSettings.infoPageAutoCyclePagesMask =
      doc["auto_cycle_pages_mask"] | kDefaultInfoPageAutoCyclePagesMask;
  gAutoViewSettings.infoPageAutoCyclePhotoCount =
      doc["auto_cycle_photo_count"] | kDefaultInfoPageAutoCyclePhotoCount;
  gAutoViewSettings.infoPageAutoCycleDurationMs =
      doc["auto_cycle_duration_ms"] | kDefaultInfoPageAutoCycleDurationMs;
  gAutoViewSettings.photoRefreshIntervalMs = doc["photo_refresh_ms"] | kDefaultPhotoRefreshIntervalMs;
  gAutoViewSettings.photoFillMode = doc["photo_fill_mode"] | kDefaultPhotoFillMode;
  copyCString(gWeatherLocationLabel, sizeof(gWeatherLocationLabel), doc["weather_label"] | kDefaultWeatherLocationLabel);
  gWeatherLatitude = doc["weather_latitude"] | kDefaultWeatherLatitude;
  gWeatherLongitude = doc["weather_longitude"] | kDefaultWeatherLongitude;
  copyCString(gAgendaScriptUrl, sizeof(gAgendaScriptUrl), doc["agenda_url"] | kDefaultAgendaScriptUrl);
  gDisplayTransform = doc["display_transform"] | kDefaultDisplayTransform;

  if (doc["alarms"].is<JsonArray>()) {
    JsonArray alarms = doc["alarms"].as<JsonArray>();
    gAlarmCount = 0;
    for (JsonObject alarm : alarms) {
      if (gAlarmCount >= kMaxAlarms) {
        break;
      }
      AlarmEntry& dst = gAlarms[gAlarmCount++];
      dst.enabled = alarm["enabled"] | false;
      dst.soundEnabled = alarm["sound_enabled"] | false;
      dst.hour = alarm["hour"] | 7;
      dst.minute = alarm["minute"] | 0;
      dst.repeatMode = alarm["repeat_mode"] | static_cast<uint8_t>(AlarmRepeatMode::kDaily);
      dst.repeatDaysMask = alarm["repeat_days_mask"] | kWeekdayMaskAll;
      dst.oneTimeYear = alarm["one_time_year"] | 0;
      dst.oneTimeMonth = alarm["one_time_month"] | 0;
      dst.oneTimeDay = alarm["one_time_day"] | 0;
      copyCString(dst.label, sizeof(dst.label), alarm["label"] | "");
    }
  } else {
    // Backward compatibility with the old single-alarm schema.
    gAlarmCount = 1;
    gAlarms[0].enabled = doc["alarm_enabled"] | false;
    gAlarms[0].soundEnabled = false;
    gAlarms[0].hour = doc["alarm_hour"] | 7;
    gAlarms[0].minute = doc["alarm_minute"] | 0;
    gAlarms[0].repeatMode = static_cast<uint8_t>(AlarmRepeatMode::kDaily);
    gAlarms[0].repeatDaysMask = kWeekdayMaskAll;
    gAlarms[0].oneTimeYear = 0;
    gAlarms[0].oneTimeMonth = 0;
    gAlarms[0].oneTimeDay = 0;
    copyCString(gAlarms[0].label, sizeof(gAlarms[0].label), doc["alarm_label"] | "Alarm 1");
  }
  gAgendaEventAlarmEnabled = doc["agenda_event_alarm_enabled"] | true;
  gAgendaEventSoundEnabled = doc["agenda_event_sound"] | false;
  gAgendaEventLeadMinutes = doc["agenda_event_lead_minutes"] | 0;

  normalizeAutoViewSettings();
  Serial.println("Settings: loaded from /settings.json");
  return true;
}

bool saveAutoViewSettings() {
  normalizeAutoViewSettings();

  if (!LittleFS.begin(false)) {
    Serial.println("Settings: LittleFS mount failed; save skipped");
    return false;
  }

  DynamicJsonDocument doc(4096);
  doc["auto_timeout_enabled"] = gAutoViewSettings.infoPageAutoTimeoutEnabled;
  doc["auto_timeout_ms"] = gAutoViewSettings.infoPageAutoTimeoutMs;
  doc["auto_cycle_enabled"] = gAutoViewSettings.infoPageAutoCycleEnabled;
  doc["auto_cycle_pages_mask"] = gAutoViewSettings.infoPageAutoCyclePagesMask;
  doc["auto_cycle_photo_count"] = gAutoViewSettings.infoPageAutoCyclePhotoCount;
  doc["auto_cycle_duration_ms"] = gAutoViewSettings.infoPageAutoCycleDurationMs;
  doc["photo_refresh_ms"] = gAutoViewSettings.photoRefreshIntervalMs;
  doc["photo_fill_mode"] = gAutoViewSettings.photoFillMode;
  doc["weather_label"] = gWeatherLocationLabel;
  doc["weather_latitude"] = gWeatherLatitude;
  doc["weather_longitude"] = gWeatherLongitude;
  doc["agenda_url"] = gAgendaScriptUrl;
  doc["display_transform"] = gDisplayTransform;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < gAlarmCount; ++i) {
    JsonObject alarm = alarms.createNestedObject();
    alarm["enabled"] = gAlarms[i].enabled;
    alarm["sound_enabled"] = gAlarms[i].soundEnabled;
    alarm["hour"] = gAlarms[i].hour;
    alarm["minute"] = gAlarms[i].minute;
    alarm["repeat_mode"] = gAlarms[i].repeatMode;
    alarm["repeat_days_mask"] = gAlarms[i].repeatDaysMask;
    alarm["one_time_year"] = gAlarms[i].oneTimeYear;
    alarm["one_time_month"] = gAlarms[i].oneTimeMonth;
    alarm["one_time_day"] = gAlarms[i].oneTimeDay;
    alarm["label"] = gAlarms[i].label;
  }
  doc["agenda_event_alarm_enabled"] = gAgendaEventAlarmEnabled;
  doc["agenda_event_sound"] = gAgendaEventSoundEnabled;
  doc["agenda_event_lead_minutes"] = gAgendaEventLeadMinutes;

  File file = LittleFS.open(kSettingsJsonPath, "w");
  if (!file) {
    Serial.println("Settings: failed to open /settings.json for write");
    return false;
  }

  const size_t written = serializeJsonPretty(doc, file);
  file.close();
  if (written == 0) {
    Serial.println("Settings: failed to write settings JSON");
    return false;
  }

  Serial.println("Settings: saved to /settings.json");
  return true;
}
}  // namespace Config
