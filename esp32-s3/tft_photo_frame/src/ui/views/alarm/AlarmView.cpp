#include "ui/views/alarm/AlarmView.h"

#include <string.h>

AlarmView::AlarmView(LvglHost& host, AlarmScheduler& alarmScheduler)
    : host_(host),
      alarmScheduler_(alarmScheduler),
      uiReady_(false),
      lastRefreshMs_(0),
      screen_(nullptr),
      lblTitle_(nullptr),
      lblActive_(nullptr),
      lblRows_{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
      popupCard_(nullptr),
      lblPopupBadge_(nullptr),
      lblPopupTime_(nullptr),
      lblPopupLabel_(nullptr),
      lblPopupHint_(nullptr),
      lblHint1_(nullptr),
      lblHint2_(nullptr) {}

void AlarmView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  refreshUi();
  host_.loadScreen(screen_);
}

void AlarmView::onExit() {}

void AlarmView::update(uint32_t nowMs) {
  if (!uiReady_) {
    return;
  }

  if (lastRefreshMs_ == 0 || (nowMs - lastRefreshMs_) >= kRefreshIntervalMs) {
    refreshUi();
    lastRefreshMs_ = nowMs;
  }
}

void AlarmView::render() {
  host_.service();
}

ViewAction AlarmView::handleEvent(const PageEvent& event) {
  (void)event;
  return ViewAction::kNone;
}

void AlarmView::ensureUi() {
  if (uiReady_) {
    return;
  }
  createUi();
}

void AlarmView::createUi() {
  if (screen_ != nullptr) {
    return;
  }

  screen_ = lv_obj_create(nullptr);
  if (screen_ == nullptr) {
    return;
  }

  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x06080D), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

  lblTitle_ = lv_label_create(screen_);
  lv_label_set_text(lblTitle_, "Alarms");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 12, 8);

  lblActive_ = lv_label_create(screen_);
  lv_obj_set_width(lblActive_, 216);
  lv_label_set_long_mode(lblActive_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(lblActive_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblActive_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblActive_, LV_ALIGN_TOP_LEFT, 12, 34);

  popupCard_ = lv_obj_create(screen_);
  lv_obj_set_size(popupCard_, 216, 110);
  lv_obj_align(popupCard_, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_clear_flag(popupCard_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(popupCard_, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(popupCard_, lv_color_hex(0x111827), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(popupCard_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(popupCard_, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(popupCard_, lv_color_hex(0xFB7185), LV_PART_MAIN);
  lv_obj_set_style_pad_left(popupCard_, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(popupCard_, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_top(popupCard_, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(popupCard_, 8, LV_PART_MAIN);
  lv_obj_add_flag(popupCard_, LV_OBJ_FLAG_HIDDEN);

  lblPopupBadge_ = lv_label_create(popupCard_);
  lv_label_set_text(lblPopupBadge_, "ALARM - BEEP");
  lv_obj_set_style_text_font(lblPopupBadge_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblPopupBadge_, lv_color_hex(0xFDA4AF), LV_PART_MAIN);
  lv_obj_align(lblPopupBadge_, LV_ALIGN_TOP_LEFT, 0, 0);

  lblPopupTime_ = lv_label_create(popupCard_);
  lv_label_set_text(lblPopupTime_, "--:--");
  lv_obj_set_style_text_font(lblPopupTime_, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblPopupTime_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblPopupTime_, LV_ALIGN_TOP_LEFT, 0, 18);

  lblPopupLabel_ = lv_label_create(popupCard_);
  lv_obj_set_width(lblPopupLabel_, 194);
  lv_label_set_long_mode(lblPopupLabel_, LV_LABEL_LONG_DOT);
  lv_label_set_text(lblPopupLabel_, "Alert");
  lv_obj_set_style_text_font(lblPopupLabel_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblPopupLabel_, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
  lv_obj_align(lblPopupLabel_, LV_ALIGN_TOP_LEFT, 0, 66);

  lblPopupHint_ = lv_label_create(popupCard_);
  lv_label_set_text(lblPopupHint_, "Touch to dismiss");
  lv_obj_set_style_text_font(lblPopupHint_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblPopupHint_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblPopupHint_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  for (uint8_t i = 0; i < kAlarmRows; ++i) {
    lblRows_[i] = lv_label_create(screen_);
    lv_obj_set_style_text_font(lblRows_[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblRows_[i], lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_width(lblRows_[i], 216);
    lv_label_set_long_mode(lblRows_[i], LV_LABEL_LONG_DOT);
    lv_obj_align(lblRows_[i], LV_ALIGN_TOP_LEFT, 12, static_cast<int16_t>(60 + (i * 20)));
  }

  lblHint1_ = lv_label_create(screen_);
  lv_label_set_text(lblHint1_, "Tap TOUCH: Next page");
  lv_obj_set_style_text_font(lblHint1_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint1_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint1_, LV_ALIGN_BOTTOM_LEFT, 12, -28);

  lblHint2_ = lv_label_create(screen_);
  lv_label_set_text(lblHint2_, "Waiting for next trigger");
  lv_obj_set_style_text_font(lblHint2_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint2_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint2_, LV_ALIGN_BOTTOM_LEFT, 12, -8);

  uiReady_ = (lblTitle_ != nullptr && lblActive_ != nullptr && popupCard_ != nullptr && lblPopupBadge_ != nullptr &&
              lblPopupTime_ != nullptr && lblPopupLabel_ != nullptr && lblPopupHint_ != nullptr &&
              lblHint1_ != nullptr && lblHint2_ != nullptr);
  if (uiReady_) {
    for (uint8_t i = 0; i < kAlarmRows; ++i) {
      if (lblRows_[i] == nullptr) {
        uiReady_ = false;
        break;
      }
    }
  }

  refreshUi();
}

void AlarmView::refreshUi() {
  if (!uiReady_) {
    return;
  }

  const bool hasActive = alarmScheduler_.hasActiveAlert();
  if (hasActive) {
    const bool fromEvent = alarmScheduler_.activeAlertFromEvent();
    const bool soundEnabled = alarmScheduler_.activeAlertSoundEnabled();
    const lv_color_t accent = fromEvent ? lv_color_hex(0x60A5FA) : lv_color_hex(0xFB7185);

    lv_label_set_text(lblActive_, "Active alert");
    lv_obj_set_style_text_color(lblActive_, accent, LV_PART_MAIN);
    lv_label_set_text(lblHint2_, "Tap TOUCH: Dismiss alert");

    char badge[40];
    snprintf(badge, sizeof(badge), "%s - %s", fromEvent ? "EVENT" : "ALARM", soundEnabled ? "BEEP" : "VIRTUAL");
    lv_label_set_text(lblPopupBadge_, badge);

    char timeText[8];
    snprintf(timeText, sizeof(timeText), "%02u:%02u", static_cast<unsigned>(alarmScheduler_.activeAlertHour()),
             static_cast<unsigned>(alarmScheduler_.activeAlertMinute()));
    lv_label_set_text(lblPopupTime_, timeText);

    const char* labelText = alarmScheduler_.activeAlertLabel();
    if (labelText == nullptr || labelText[0] == '\0') {
      labelText = fromEvent ? "Calendar event" : "Alarm";
    }
    lv_label_set_text(lblPopupLabel_, labelText);
    lv_label_set_text(lblPopupHint_, soundEnabled ? "Touch to dismiss and stop beep" : "Touch to dismiss");

    lv_obj_set_style_border_color(popupCard_, accent, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblPopupBadge_, accent, LV_PART_MAIN);
    lv_obj_clear_flag(popupCard_, LV_OBJ_FLAG_HIDDEN);

    for (uint8_t i = 0; i < kAlarmRows; ++i) {
      if (lblRows_[i] != nullptr) {
        lv_obj_add_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  lv_obj_add_flag(popupCard_, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lblActive_, "No active alarm");
  lv_obj_set_style_text_color(lblActive_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_label_set_text(lblHint2_, "Waiting for next trigger");

  const uint8_t configuredCount =
      (Config::gAlarmCount <= Config::kMaxAlarms) ? Config::gAlarmCount : Config::kMaxAlarms;

  char line[160];
  char repeatText[64];
  if (configuredCount == 0) {
    if (lblRows_[0] != nullptr) {
      lv_obj_clear_flag(lblRows_[0], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(lblRows_[0], "No alarms configured in web.");
      lv_obj_set_style_text_color(lblRows_[0], lv_color_hex(0x93C5FD), LV_PART_MAIN);
    }
    for (uint8_t i = 1; i < kAlarmRows; ++i) {
      if (lblRows_[i] != nullptr) {
        lv_obj_add_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  for (uint8_t i = 0; i < kAlarmRows; ++i) {
    if (lblRows_[i] == nullptr) {
      continue;
    }

    if (i >= configuredCount) {
      lv_obj_add_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    lv_obj_clear_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);
    const Config::AlarmEntry& alarm = Config::gAlarms[i];

    const uint8_t repeatMode = alarm.repeatMode;
    if (repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kDaily)) {
      snprintf(repeatText, sizeof(repeatText), "Daily");
    } else if (repeatMode == static_cast<uint8_t>(Config::AlarmRepeatMode::kWeekdays)) {
      char days[40];
      weekdayMaskToText(alarm.repeatDaysMask, days, sizeof(days));
      snprintf(repeatText, sizeof(repeatText), "Wk:%s", days);
    } else {
      snprintf(repeatText, sizeof(repeatText), "Once %04u-%02u-%02u", static_cast<unsigned>(alarm.oneTimeYear),
               static_cast<unsigned>(alarm.oneTimeMonth), static_cast<unsigned>(alarm.oneTimeDay));
    }

    if (alarm.label[0] != '\0') {
      snprintf(line, sizeof(line), "%u. %02u:%02u %s %s %s %s", static_cast<unsigned>(i + 1),
               static_cast<unsigned>(alarm.hour), static_cast<unsigned>(alarm.minute), alarm.enabled ? "ON" : "OFF",
               alarm.soundEnabled ? "BEEP" : "VIRTUAL", repeatText, alarm.label);
    } else {
      snprintf(line, sizeof(line), "%u. %02u:%02u %s %s %s", static_cast<unsigned>(i + 1),
               static_cast<unsigned>(alarm.hour), static_cast<unsigned>(alarm.minute), alarm.enabled ? "ON" : "OFF",
               alarm.soundEnabled ? "BEEP" : "VIRTUAL", repeatText);
    }
    lv_label_set_text(lblRows_[i], line);
    lv_obj_set_style_text_color(lblRows_[i], alarm.enabled ? lv_color_hex(0xD1D5DB) : lv_color_hex(0x6B7280),
                                LV_PART_MAIN);
  }
}

void AlarmView::weekdayMaskToText(uint8_t mask, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return;
  }

  out[0] = '\0';
  static const char* kDays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  bool first = true;
  for (uint8_t i = 0; i < 7; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((mask & bit) == 0) {
      continue;
    }

    if (!first) {
      strncat(out, ",", outSize - strlen(out) - 1);
    }
    strncat(out, kDays[i], outSize - strlen(out) - 1);
    first = false;
  }

  if (out[0] == '\0') {
    snprintf(out, outSize, "-");
  }
}
