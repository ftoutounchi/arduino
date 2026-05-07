#include "ui/views/dashboard/DashboardView.h"

#include <time.h>

#include "ui/assets/WeatherIcons.h"

namespace {
const char* weatherCodeToShortText(int code) {
  switch (code) {
    case 0:
      return "Clear";
    case 1:
      return "Mostly Clear";
    case 2:
      return "Partly Cloudy";
    case 3:
      return "Overcast";
    case 45:
    case 48:
      return "Fog";
    case 51:
    case 53:
    case 55:
      return "Drizzle";
    case 56:
    case 57:
      return "Freezing Drizzle";
    case 61:
    case 63:
    case 65:
      return "Rain";
    case 66:
    case 67:
      return "Freezing Rain";
    case 71:
    case 73:
    case 75:
    case 77:
      return "Snow";
    case 80:
    case 81:
    case 82:
      return "Rain Showers";
    case 85:
    case 86:
      return "Snow Showers";
    case 95:
      return "Thunderstorm";
    case 96:
    case 99:
      return "Tstorm+Hail";
    default:
      return "Weather";
  }
}

void formatOneDecimal(char* out, size_t outSize, float value, const char* suffix) {
  if (out == nullptr || outSize == 0) {
    return;
  }

  const long scaled = static_cast<long>((value * 10.0f) + ((value < 0.0f) ? -0.5f : 0.5f));
  const unsigned long absScaled = static_cast<unsigned long>((scaled < 0) ? -scaled : scaled);
  const unsigned long whole = absScaled / 10UL;
  const unsigned long fraction = absScaled % 10UL;
  const char* safeSuffix = (suffix == nullptr) ? "" : suffix;

  if (scaled < 0) {
    snprintf(out, outSize, "-%lu.%lu%s", whole, fraction, safeSuffix);
  } else {
    snprintf(out, outSize, "%lu.%lu%s", whole, fraction, safeSuffix);
  }
}

bool currentLocalNight() {
  struct tm timeInfo = {};
  if (!getLocalTime(&timeInfo, 20)) {
    return false;
  }
  return (timeInfo.tm_hour < 6 || timeInfo.tm_hour >= 18);
}

void setWeatherIconSrc(lv_obj_t* imageObj, int weatherCode, bool night, bool large) {
  if (imageObj == nullptr) {
    return;
  }
  lv_img_set_src(imageObj, weatherIconForCode(weatherCode, night, large));
}
}  // namespace

DashboardView::DashboardView(LvglHost& host, WeatherService& weatherService)
    : host_(host),
      weatherService_(weatherService),
      uiReady_(false),
      needImmediateRefresh_(false),
      lastClockUpdateMs_(0),
      lastWeatherUpdateMs_(0),
      lastWeatherAttemptMs_(0),
      screen_(nullptr),
      lblTitle_(nullptr),
      lblDate_(nullptr),
      lblYear_(nullptr),
      lblTime_(nullptr),
      weatherIconImg_(nullptr),
      lblLocation_(nullptr),
      lblCondition_(nullptr),
      lblTemp_(nullptr),
      lblHumidity_(nullptr),
      lblPressure_(nullptr),
      lblWind_(nullptr),
      lblForecastDay_{nullptr, nullptr, nullptr, nullptr},
      forecastIconImg_{nullptr, nullptr, nullptr, nullptr},
      lblForecastTemp_{nullptr, nullptr, nullptr, nullptr},
      lblHint_(nullptr) {}

void DashboardView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  updateClockLabel();
  const uint32_t now = millis();
  const uint32_t weatherInterval = (lastWeatherUpdateMs_ == 0) ? 0 : (now - lastWeatherUpdateMs_);
  const uint32_t weatherRetry = (lastWeatherAttemptMs_ == 0) ? 0 : (now - lastWeatherAttemptMs_);
  const bool dueRefresh = (lastWeatherUpdateMs_ == 0) || (weatherInterval >= Config::kWeatherRefreshIntervalMs);
  const bool retryWindowOk = (lastWeatherAttemptMs_ == 0) || (weatherRetry >= Config::kWeatherRetryIntervalMs);
  if (dueRefresh && retryWindowOk) {
    updateWeatherLabel();
    lastWeatherAttemptMs_ = now;
  }

  host_.loadScreen(screen_);
  needImmediateRefresh_ = true;
}

void DashboardView::onExit() {}

void DashboardView::update(uint32_t nowMs) {
  if (!uiReady_) {
    return;
  }

  if (needImmediateRefresh_ || (nowMs - lastClockUpdateMs_) >= Config::kInfoClockUpdateIntervalMs) {
    updateClockLabel();
    lastClockUpdateMs_ = nowMs;
  }

  const uint32_t weatherInterval = (lastWeatherUpdateMs_ == 0) ? 0 : (nowMs - lastWeatherUpdateMs_);
  const uint32_t weatherRetry = (lastWeatherAttemptMs_ == 0) ? 0 : (nowMs - lastWeatherAttemptMs_);
  const bool dueRefresh = needImmediateRefresh_ || lastWeatherUpdateMs_ == 0 ||
                          weatherInterval >= Config::kWeatherRefreshIntervalMs;
  const bool retryWindowOk = lastWeatherAttemptMs_ == 0 || weatherRetry >= Config::kWeatherRetryIntervalMs;
  if (dueRefresh && retryWindowOk) {
    updateWeatherLabel();
    lastWeatherAttemptMs_ = nowMs;
  }

  needImmediateRefresh_ = false;
}

void DashboardView::render() {
  host_.service();
}

ViewAction DashboardView::handleEvent(const PageEvent& event) {
  (void)event;
  return ViewAction::kNone;
}

void DashboardView::ensureUi() {
  if (uiReady_) {
    return;
  }
  createUi();
}

void DashboardView::createUi() {
  if (screen_ != nullptr) {
    return;
  }

  screen_ = lv_obj_create(nullptr);
  if (screen_ == nullptr) {
    return;
  }
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

  lblTitle_ = lv_label_create(screen_);
  lblDate_ = lv_label_create(screen_);
  lblYear_ = lv_label_create(screen_);
  lblTime_ = lv_label_create(screen_);
  weatherIconImg_ = lv_img_create(screen_);
  lblLocation_ = lv_label_create(screen_);
  lblCondition_ = lv_label_create(screen_);
  lblTemp_ = lv_label_create(screen_);
  lblHumidity_ = lv_label_create(screen_);
  lblPressure_ = lv_label_create(screen_);
  lblWind_ = lv_label_create(screen_);
  for (uint8_t i = 0; i < kForecastSlots; ++i) {
    lblForecastDay_[i] = lv_label_create(screen_);
    forecastIconImg_[i] = lv_img_create(screen_);
    lblForecastTemp_[i] = lv_label_create(screen_);
  }
  lblHint_ = lv_label_create(screen_);

  if (lblTitle_ == nullptr || lblDate_ == nullptr || lblYear_ == nullptr || lblTime_ == nullptr ||
      weatherIconImg_ == nullptr || lblLocation_ == nullptr || lblCondition_ == nullptr || lblTemp_ == nullptr ||
      lblHumidity_ == nullptr || lblPressure_ == nullptr || lblWind_ == nullptr || lblHint_ == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < kForecastSlots; ++i) {
    if (lblForecastDay_[i] == nullptr || forecastIconImg_[i] == nullptr || lblForecastTemp_[i] == nullptr) {
      return;
    }
  }

  lv_label_set_text(lblTitle_, "Sunday");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xDCE7FF), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 12, 10);

  lv_label_set_text(lblDate_, "--.--");
  lv_obj_set_style_text_font(lblDate_, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblDate_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblDate_, LV_ALIGN_TOP_LEFT, 12, 30);

  lv_label_set_text(lblYear_, "----");
  lv_obj_set_style_text_font(lblYear_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblYear_, lv_color_hex(0xDCE7FF), LV_PART_MAIN);
  lv_obj_align(lblYear_, LV_ALIGN_TOP_LEFT, 50, 60);

  lv_label_set_text(lblTime_, "--:--");
  lv_obj_set_style_text_font(lblTime_, &lv_font_montserrat_40, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTime_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblTime_, LV_ALIGN_TOP_RIGHT, -12, 20);

  lv_obj_set_size(weatherIconImg_, kWeatherIconSize, kWeatherIconSize);
  lv_obj_align(weatherIconImg_, LV_ALIGN_TOP_LEFT, 20, 78);
  drawWeatherIcon(0, false);

  lv_label_set_text(lblLocation_, Config::gWeatherLocationLabel);
  lv_obj_set_style_text_font(lblLocation_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblLocation_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblLocation_, LV_ALIGN_TOP_RIGHT, -12, 78);

  lv_label_set_text(lblCondition_, "Clear");
  lv_obj_set_style_text_font(lblCondition_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblCondition_, lv_color_hex(0xDDE7F5), LV_PART_MAIN);
  lv_obj_align(lblCondition_, LV_ALIGN_TOP_RIGHT, 0, 85);

  lv_label_set_text(lblTemp_, "--.-");
  lv_obj_set_style_text_font(lblTemp_, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTemp_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblTemp_, LV_ALIGN_TOP_RIGHT, 0, 115);

  lv_label_set_text(lblHumidity_, "RH --%");
  lv_obj_set_style_text_font(lblHumidity_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHumidity_, lv_color_hex(0xC7D2FE), LV_PART_MAIN);
  lv_obj_align(lblHumidity_, LV_ALIGN_TOP_LEFT, 14, 160);

  lv_label_set_text(lblPressure_, "--- hPa");
  lv_obj_set_style_text_font(lblPressure_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblPressure_, lv_color_hex(0xC7D2FE), LV_PART_MAIN);
  lv_obj_align(lblPressure_, LV_ALIGN_TOP_MID, 0, 160);

  lv_label_set_text(lblWind_, "--.- m/s");
  lv_obj_set_style_text_font(lblWind_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblWind_, lv_color_hex(0xC7D2FE), LV_PART_MAIN);
  lv_obj_align(lblWind_, LV_ALIGN_TOP_RIGHT, -12, 160);

  for (uint8_t i = 0; i < kForecastSlots; ++i) {
    const int16_t x = static_cast<int16_t>(-84 + (i * 56));
    lv_label_set_text(lblForecastDay_[i], "---");
    lv_obj_set_style_text_font(lblForecastDay_[i], &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblForecastDay_[i], lv_color_hex(0xDDE7F5), LV_PART_MAIN);
    lv_obj_align(lblForecastDay_[i], LV_ALIGN_TOP_MID, x, 184);

    lv_obj_set_size(forecastIconImg_[i], kForecastIconSize, kForecastIconSize);
    lv_obj_align(forecastIconImg_[i], LV_ALIGN_TOP_MID, x, 200);
    setWeatherIconSrc(forecastIconImg_[i], -1, false, false);

    lv_label_set_text(lblForecastTemp_[i], "--C");
    lv_obj_set_style_text_font(lblForecastTemp_[i], &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblForecastTemp_[i], lv_color_hex(0xDDE7F5), LV_PART_MAIN);
    lv_obj_align(lblForecastTemp_[i], LV_ALIGN_TOP_MID, x, 256);
  }

  uiReady_ = true;
}

void DashboardView::updateClockLabel() {
  if (lblTime_ == nullptr || lblTitle_ == nullptr || lblDate_ == nullptr || lblYear_ == nullptr) {
    return;
  }

  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 20)) {
    static const char* kWeekdayNames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                           "Thursday", "Friday", "Saturday"};
    lv_label_set_text_fmt(lblTime_, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
    lv_label_set_text(lblTitle_, kWeekdayNames[timeInfo.tm_wday]);
    lv_label_set_text_fmt(lblDate_, "%02d.%02d", timeInfo.tm_mday, timeInfo.tm_mon + 1);
    lv_label_set_text_fmt(lblYear_, "%04d", timeInfo.tm_year + 1900);
    setHint("Press TOUCH to return");
  } else {
    lv_label_set_text(lblTime_, "--:--");
    lv_label_set_text(lblTitle_, "----");
    lv_label_set_text(lblDate_, "--.--");
    lv_label_set_text(lblYear_, "----");
    setHint("Waiting for NTP sync");
  }
}

void DashboardView::updateWeatherLabel() {
  if (lblCondition_ == nullptr || lblTemp_ == nullptr || lblHumidity_ == nullptr || lblPressure_ == nullptr ||
      lblWind_ == nullptr) {
    return;
  }
  if (lblLocation_ != nullptr) {
    lv_label_set_text(lblLocation_, Config::gWeatherLocationLabel);
  }

  WeatherService::Result result = {};
  weatherService_.fetch(&result);

  if (!result.success) {
    drawWeatherIcon(95, currentLocalNight());
    lv_label_set_text(lblCondition_, result.errorText.c_str());
    lv_obj_set_style_text_color(lblCondition_, lv_color_hex(0xFCA5A5), LV_PART_MAIN);
    lv_label_set_text(lblTemp_, "--.-");
    lv_label_set_text(lblHumidity_, "RH --%");
    lv_label_set_text(lblPressure_, "--- hPa");
    lv_label_set_text(lblWind_, "--.- m/s");
    clearForecast();
    return;
  }

  const bool night = currentLocalNight();
  if (result.hasWeatherCode) {
    drawWeatherIcon(result.weatherCode, night);
    lv_label_set_text(lblCondition_, weatherCodeToShortText(result.weatherCode));
  } else {
    drawWeatherIcon(-1, night);
    lv_label_set_text(lblCondition_, "Weather");
  }
  lv_obj_set_style_text_color(lblCondition_, lv_color_hex(0xDDE7F5), LV_PART_MAIN);

  char rh[16];
  char hp[16];
  char ws[16];
  if (result.hasTemp) {
    char temp[20];
    formatOneDecimal(temp, sizeof(temp), result.tempC, " C");
    lv_label_set_text(lblTemp_, temp);
  } else {
    lv_label_set_text(lblTemp_, "--.-");
  }
  if (result.hasHumidity) {
    snprintf(rh, sizeof(rh), "RH %d%%", result.humidityPct);
  } else {
    snprintf(rh, sizeof(rh), "RH --%%");
  }
  if (result.hasPressure) {
    snprintf(hp, sizeof(hp), "%d hPa", result.pressureHpa);
  } else {
    snprintf(hp, sizeof(hp), "--- hPa");
  }
  if (result.hasWind) {
    formatOneDecimal(ws, sizeof(ws), result.windMs, " m/s");
  } else {
    snprintf(ws, sizeof(ws), "--.- m/s");
  }
  lv_label_set_text(lblHumidity_, rh);
  lv_label_set_text(lblPressure_, hp);
  lv_label_set_text(lblWind_, ws);

  clearForecast();
  const uint8_t forecastLimit = (result.forecastCount <= kForecastSlots) ? result.forecastCount : kForecastSlots;
  for (uint8_t i = 0; i < forecastLimit; ++i) {
    lv_label_set_text(lblForecastDay_[i], result.forecast[i].dayLabel.c_str());
    if (result.forecast[i].hasMaxTemp) {
      lv_label_set_text_fmt(lblForecastTemp_[i], "%dC", result.forecast[i].maxTempC);
    } else {
      lv_label_set_text(lblForecastTemp_[i], "--C");
    }

    const int code = result.forecast[i].hasWeatherCode ? result.forecast[i].weatherCode : -1;
    setWeatherIconSrc(forecastIconImg_[i], code, false, false);
  }

  lastWeatherUpdateMs_ = millis();
}

void DashboardView::setHint(const char* text) {
  if (lblHint_ == nullptr || text == nullptr) {
    return;
  }
  lv_label_set_text(lblHint_, text);
}

void DashboardView::clearForecast() {
  for (uint8_t i = 0; i < kForecastSlots; ++i) {
    if (lblForecastDay_[i] != nullptr) {
      lv_label_set_text(lblForecastDay_[i], "---");
    }
    if (lblForecastTemp_[i] != nullptr) {
      lv_label_set_text(lblForecastTemp_[i], "--C");
    }
    if (forecastIconImg_[i] != nullptr) {
      setWeatherIconSrc(forecastIconImg_[i], -1, false, false);
    }
  }
}

void DashboardView::drawWeatherIcon(int code, bool night) {
  setWeatherIconSrc(weatherIconImg_, code, night, true);
}
