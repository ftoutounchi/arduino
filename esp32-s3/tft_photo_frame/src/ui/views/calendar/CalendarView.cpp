#include "ui/views/calendar/CalendarView.h"

#include <time.h>

CalendarView::CalendarView(LvglHost& host)
    : host_(host),
      uiReady_(false),
      needImmediateRefresh_(false),
      lastRefreshMs_(0),
      screen_(nullptr),
      lblTitle_(nullptr),
      lblMonth_(nullptr),
      table_(nullptr),
      lblHint_(nullptr) {}

void CalendarView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  resetTable();
  updateCalendarText();
  host_.loadScreen(screen_);
  needImmediateRefresh_ = true;
}

void CalendarView::onExit() {}

void CalendarView::update(uint32_t nowMs) {
  if (!uiReady_) {
    return;
  }

  constexpr uint32_t kCalendarRefreshMs = 5000;
  if (needImmediateRefresh_ || (nowMs - lastRefreshMs_) >= kCalendarRefreshMs) {
    updateCalendarText();
    lastRefreshMs_ = nowMs;
    needImmediateRefresh_ = false;
  }
}

void CalendarView::render() {
  host_.service();
}

ViewAction CalendarView::handleEvent(const PageEvent& event) {
  (void)event;
  return ViewAction::kNone;
}

void CalendarView::ensureUi() {
  if (uiReady_) {
    return;
  }
  createUi();
}

void CalendarView::createUi() {
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
  lv_label_set_text(lblTitle_, "Calendar");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 10, 10);

  lblMonth_ = lv_label_create(screen_);
  lv_label_set_text(lblMonth_, "---- ----");
  lv_obj_set_style_text_font(lblMonth_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblMonth_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblMonth_, LV_ALIGN_TOP_RIGHT, -10, 10);

  table_ = lv_table_create(screen_);
  lv_obj_set_size(table_, 220, 196);
  lv_obj_align(table_, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_clear_flag(table_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(table_, LV_OBJ_FLAG_CLICKABLE);
  lv_table_set_col_cnt(table_, 7);
  lv_table_set_row_cnt(table_, 7);
  for (uint8_t col = 0; col < 7; ++col) {
    lv_table_set_col_width(table_, col, 31);
  }

  lv_obj_set_style_pad_all(table_, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(table_, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(table_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(table_, lv_color_hex(0x334155), LV_PART_MAIN);
  lv_obj_set_style_border_width(table_, 1, LV_PART_MAIN);

  lv_obj_set_style_bg_color(table_, lv_color_hex(0x000000), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(table_, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(table_, lv_color_hex(0x334155), LV_PART_ITEMS);
  lv_obj_set_style_border_width(table_, 1, LV_PART_ITEMS);
  lv_obj_set_style_text_font(table_, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_set_style_text_color(table_, lv_color_hex(0xD1D5DB), LV_PART_ITEMS);
  lv_obj_set_style_text_align(table_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(table_, 2, LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(table_, 2, LV_PART_ITEMS);

  lv_obj_set_style_bg_color(table_, lv_color_hex(0x1E3A8A), LV_PART_ITEMS | LV_TABLE_CELL_CTRL_CUSTOM_1);
  lv_obj_set_style_bg_opa(table_, LV_OPA_COVER, LV_PART_ITEMS | LV_TABLE_CELL_CTRL_CUSTOM_1);
  lv_obj_set_style_text_color(table_, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_TABLE_CELL_CTRL_CUSTOM_1);

  lblHint_ = lv_label_create(screen_);
  lv_label_set_text(lblHint_, "Press TOUCH: Next page");
  lv_obj_set_style_text_font(lblHint_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint_, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  uiReady_ = (lblTitle_ != nullptr && lblMonth_ != nullptr && table_ != nullptr && lblHint_ != nullptr);
}

void CalendarView::resetTable() {
  if (table_ == nullptr) {
    return;
  }

  lv_table_set_col_cnt(table_, 7);
  lv_table_set_row_cnt(table_, 7);
  for (uint8_t col = 0; col < 7; ++col) {
    lv_table_set_col_width(table_, col, 31);
  }
  for (uint8_t row = 0; row < 7; ++row) {
    for (uint8_t col = 0; col < 7; ++col) {
      lv_table_set_cell_value(table_, row, col, "");
      lv_table_clear_cell_ctrl(table_, row, col, LV_TABLE_CELL_CTRL_CUSTOM_1);
    }
  }
}

void CalendarView::updateCalendarText() {
  if (lblMonth_ == nullptr || table_ == nullptr) {
    return;
  }

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 20)) {
    lv_label_set_text(lblMonth_, "Time not ready");
    for (uint8_t row = 0; row < 7; ++row) {
      for (uint8_t col = 0; col < 7; ++col) {
        lv_table_set_cell_value(table_, row, col, (row == 0) ? "--" : "");
      }
    }
    return;
  }

  const int year = timeInfo.tm_year + 1900;
  const int month = timeInfo.tm_mon + 1;
  if (month < 1 || month > 12) {
    lv_label_set_text(lblMonth_, "Invalid date");
    for (uint8_t row = 0; row < 7; ++row) {
      for (uint8_t col = 0; col < 7; ++col) {
        lv_table_set_cell_value(table_, row, col, "");
      }
    }
    return;
  }

  static const char* kMonthNames[12] = {"January",   "February", "March",    "April",
                                         "May",       "June",     "July",     "August",
                                         "September", "October",  "November", "December"};
  lv_label_set_text_fmt(lblMonth_, "%s %d", kMonthNames[month - 1], year);

  auto isLeapYear = [](int currentYear) -> bool {
    return ((currentYear % 4) == 0 && (currentYear % 100) != 0) || ((currentYear % 400) == 0);
  };
  static const uint8_t kMonthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int daysInMonth = kMonthDays[month - 1];
  if (month == 2 && isLeapYear(year)) {
    daysInMonth = 29;
  }

  struct tm firstDay = {};
  firstDay.tm_year = year - 1900;
  firstDay.tm_mon = month - 1;
  firstDay.tm_mday = 1;
  firstDay.tm_hour = 12;
  firstDay.tm_isdst = -1;
  mktime(&firstDay);
  const int mondayBasedOffset = (firstDay.tm_wday + 6) % 7;

  static const char* kDays[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
  for (uint8_t col = 0; col < 7; ++col) {
    lv_table_set_cell_value(table_, 0, col, kDays[col]);
  }
  for (uint8_t row = 1; row < 7; ++row) {
    for (uint8_t col = 0; col < 7; ++col) {
      lv_table_set_cell_value(table_, row, col, "");
      lv_table_clear_cell_ctrl(table_, row, col, LV_TABLE_CELL_CTRL_CUSTOM_1);
    }
  }

  for (int day = 1; day <= daysInMonth; ++day) {
    const int idx = mondayBasedOffset + (day - 1);
    const uint8_t row = static_cast<uint8_t>(1 + (idx / 7));
    const uint8_t col = static_cast<uint8_t>(idx % 7);
    if (row >= 7) {
      continue;
    }

    char dayText[6];
    snprintf(dayText, sizeof(dayText), "%02d", day);
    lv_table_set_cell_value(table_, row, col, dayText);

    if (day == timeInfo.tm_mday) {
      lv_table_add_cell_ctrl(table_, row, col, LV_TABLE_CELL_CTRL_CUSTOM_1);
    }
  }
}
