#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace Config {
constexpr int16_t kDisplayWidth = 240;
constexpr int16_t kDisplayHeight = 280;
constexpr size_t kMaxWeatherLabelLength = 48;
constexpr size_t kMaxAgendaUrlLength = 256;
constexpr size_t kMaxAlarmLabelLength = 48;
constexpr uint8_t kMaxAlarms = 8;
constexpr uint8_t kPinSclk = 7;
constexpr uint8_t kPinMosi = 8;
constexpr uint8_t kPinRst = 9;
constexpr uint8_t kPinDc = 10;
constexpr uint8_t kPinCs = 11;
constexpr uint8_t kPinBl = 6;
constexpr uint8_t kPinBuzzer = 4;
constexpr bool kBacklightActiveHigh = true;
constexpr uint8_t kPinBootButton = 5;
constexpr int8_t kPinBootButtonAlt = -1;
// Touch modules typically idle LOW and go HIGH when touched.
constexpr bool kBootButtonActiveLow = false;
constexpr uint32_t kBootButtonDebounceMs = 20;

extern const char kWifiSsid[];
extern const char kWifiPassword[];
extern const char kPhotoUrlsJsonPath[];
extern const char kGithubAuthJsonPath[];
extern const char kSettingsJsonPath[];
extern const char kTimeZone[];
extern const char kNtpServer1[];
extern const char kNtpServer2[];
extern const char kDefaultWeatherLocationLabel[];
extern const char kDefaultAgendaScriptUrl[];
constexpr double kDefaultWeatherLatitude = 53.6060;
constexpr double kDefaultWeatherLongitude = 10.0676;
extern char gWeatherLocationLabel[kMaxWeatherLabelLength];
extern double gWeatherLatitude;
extern double gWeatherLongitude;

constexpr uint32_t kRefreshIntervalMs = 10000;
constexpr uint32_t kInfoClockUpdateIntervalMs = 1000;
constexpr uint32_t kWeatherRefreshIntervalMs = 10 * 60 * 1000;
constexpr uint32_t kWeatherRetryIntervalMs = 30 * 1000;
constexpr uint32_t kWeatherHttpTimeoutMs = 2500;
constexpr uint32_t kAgendaRefreshIntervalMs = 10 * 60 * 1000;
constexpr uint32_t kAgendaRetryIntervalMs = 30 * 1000;
constexpr uint32_t kAgendaHttpTimeoutMs = 2500;
// Auto behavior for the time/weather dashboard page.
constexpr bool kDefaultInfoPageAutoTimeoutEnabled = true;
constexpr uint32_t kDefaultInfoPageAutoTimeoutMs = 30 * 1000;
constexpr bool kDefaultInfoPageAutoCycleEnabled = true;
constexpr uint16_t kDefaultInfoPageAutoCyclePhotoCount = 2;
constexpr uint32_t kDefaultInfoPageAutoCycleDurationMs = 30 * 1000;
constexpr uint32_t kDefaultPhotoRefreshIntervalMs = kRefreshIntervalMs;
constexpr bool kDefaultPhotoFillMode = true;
constexpr uint32_t kBootButtonHoldMs = 1200;
constexpr uint8_t kDisplayTransformMax = 3;
constexpr uint8_t kDefaultDisplayTransform = 3;

struct AutoViewSettings {
  bool infoPageAutoTimeoutEnabled;
  uint32_t infoPageAutoTimeoutMs;
  bool infoPageAutoCycleEnabled;
  uint16_t infoPageAutoCyclePhotoCount;
  uint32_t infoPageAutoCycleDurationMs;
  uint32_t photoRefreshIntervalMs;
  bool photoFillMode;
};

extern AutoViewSettings gAutoViewSettings;
enum class AlarmRepeatMode : uint8_t {
  kDaily = 0,
  kWeekdays = 1,
  kOneTime = 2,
};

constexpr uint8_t kWeekdayMaskAll = 0x7F;
constexpr uint8_t kWeekdayMaskMonToFri = 0x3E;

struct AlarmEntry {
  bool enabled;
  bool soundEnabled;
  uint8_t hour;
  uint8_t minute;
  uint8_t repeatMode;
  uint8_t repeatDaysMask;
  uint16_t oneTimeYear;
  uint8_t oneTimeMonth;
  uint8_t oneTimeDay;
  char label[kMaxAlarmLabelLength];
};
extern char gAgendaScriptUrl[kMaxAgendaUrlLength];
extern AlarmEntry gAlarms[kMaxAlarms];
extern uint8_t gAlarmCount;
extern bool gAgendaEventAlarmEnabled;
extern bool gAgendaEventSoundEnabled;
extern uint16_t gAgendaEventLeadMinutes;
extern uint8_t gDisplayTransform;
bool loadAutoViewSettings();
bool saveAutoViewSettings();
void resetAutoViewSettingsToDefaults();
constexpr size_t kMaxJpegBytes = 350 * 1024;
constexpr uint32_t kDownloadTaskStack = 9000;
constexpr size_t kMaxPhotoUrls = 16;
constexpr size_t kMaxPhotoSources = 16;
constexpr size_t kMaxGithubTokenLength = 200;
constexpr size_t kMaxUserAgentLength = 64;
}  // namespace Config
