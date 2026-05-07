#include "ui/views/agenda/AgendaView.h"

AgendaView::AgendaView(LvglHost& host, AgendaService& service)
    : host_(host),
      service_(service),
      uiReady_(false),
      needImmediateRefresh_(false),
      lastAgendaUpdateMs_(0),
      lastAgendaAttemptMs_(0),
      screen_(nullptr),
      lblTitle_(nullptr),
      lblStatus_(nullptr),
      lblUpdated_(nullptr),
      lblItems_{nullptr, nullptr, nullptr, nullptr},
      lblHint_(nullptr) {}

void AgendaView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  host_.loadScreen(screen_);
  if (lblStatus_ != nullptr) {
    lv_label_set_text(lblStatus_, "Loading events...");
    lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  }
  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    if (lblItems_[i] != nullptr) {
      lv_label_set_text(lblItems_[i], "--");
    }
  }
  lastAgendaAttemptMs_ = 0;
  lastAgendaUpdateMs_ = 0;
  needImmediateRefresh_ = true;
}

void AgendaView::onExit() {}

void AgendaView::update(uint32_t nowMs) {
  if (!uiReady_) {
    return;
  }

  const uint32_t agendaInterval = (lastAgendaUpdateMs_ == 0) ? 0 : (nowMs - lastAgendaUpdateMs_);
  const uint32_t agendaRetry = (lastAgendaAttemptMs_ == 0) ? 0 : (nowMs - lastAgendaAttemptMs_);
  const bool dueRefresh = needImmediateRefresh_ || lastAgendaUpdateMs_ == 0 ||
                          agendaInterval >= Config::kAgendaRefreshIntervalMs;
  const bool retryWindowOk = lastAgendaAttemptMs_ == 0 || agendaRetry >= Config::kAgendaRetryIntervalMs;
  if (dueRefresh && retryWindowOk) {
    updateAgendaItems();
    lastAgendaAttemptMs_ = nowMs;
  }

  needImmediateRefresh_ = false;
}

void AgendaView::render() {
  host_.service();
}

ViewAction AgendaView::handleEvent(const PageEvent& event) {
  (void)event;
  return ViewAction::kNone;
}

void AgendaView::ensureUi() {
  if (uiReady_) {
    return;
  }
  createUi();
}

void AgendaView::createUi() {
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
  lv_label_set_text(lblTitle_, "Agenda");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 10, 10);

  lblStatus_ = lv_label_create(screen_);
  lv_label_set_text(lblStatus_, "Google Calendar");
  lv_obj_set_style_text_font(lblStatus_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblStatus_, LV_ALIGN_TOP_RIGHT, -10, 14);

  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    lblItems_[i] = lv_label_create(screen_);
    lv_label_set_text(lblItems_[i], "--");
    lv_obj_set_style_text_font(lblItems_[i], &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblItems_[i], lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_label_set_long_mode(lblItems_[i], LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblItems_[i], 220);
    lv_obj_align(lblItems_[i], LV_ALIGN_TOP_LEFT, 10, static_cast<lv_coord_t>(46 + (i * 46)));
  }

  lblUpdated_ = lv_label_create(screen_);
  lv_label_set_text(lblUpdated_, "Updated: --:--");
  lv_obj_set_style_text_font(lblUpdated_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblUpdated_, lv_color_hex(0x94A3B8), LV_PART_MAIN);
  lv_obj_align(lblUpdated_, LV_ALIGN_BOTTOM_LEFT, 10, -26);

  lblHint_ = lv_label_create(screen_);
  lv_label_set_text(lblHint_, "Press TOUCH: Next page");
  lv_obj_set_style_text_font(lblHint_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint_, LV_ALIGN_BOTTOM_LEFT, 10, -8);

  uiReady_ = (lblTitle_ != nullptr && lblStatus_ != nullptr && lblUpdated_ != nullptr && lblHint_ != nullptr);
  if (uiReady_) {
    for (uint8_t i = 0; i < kAgendaSlots; ++i) {
      if (lblItems_[i] == nullptr) {
        uiReady_ = false;
        break;
      }
    }
  }
}

void AgendaView::updateAgendaItems() {
  if (lblStatus_ == nullptr || lblUpdated_ == nullptr) {
    return;
  }
  AgendaService::Result result = {};
  service_.fetch(&result);

  lv_label_set_text(lblStatus_, result.statusText.c_str());
  lv_obj_set_style_text_color(lblStatus_, result.success ? lv_color_hex(0x93C5FD) : lv_color_hex(0xFCA5A5),
                              LV_PART_MAIN);
  lv_label_set_text(lblUpdated_, result.updatedText.c_str());

  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    const char* text = (i < result.lineCount) ? result.lines[i].c_str() : "--";
    if (lblItems_[i] != nullptr) {
      lv_label_set_text(lblItems_[i], text);
    }
  }

  if (result.success) {
    lastAgendaUpdateMs_ = millis();
  }
}
