#include "ui/views/agenda/AgendaView.h"

namespace {
bool parseKeyValueChunk(const String& text, const char* key, String* outValue) {
  if (key == nullptr || outValue == nullptr) {
    return false;
  }

  const String marker = String(key) + "=";
  const int keyPos = text.indexOf(marker);
  if (keyPos < 0) {
    return false;
  }

  int valueStart = keyPos + static_cast<int>(marker.length());
  while (valueStart < text.length() && text[valueStart] == ' ') {
    ++valueStart;
  }

  int valueEnd = text.length();
  for (int i = valueStart; i < text.length(); ++i) {
    const char ch = text[i];
    if (ch == ',' || ch == '}') {
      valueEnd = i;
      break;
    }
  }

  *outValue = text.substring(valueStart, valueEnd);
  outValue->trim();
  return outValue->length() > 0;
}

void parseAgendaLine(const String& rawLine, uint8_t slot, String* outIndex, String* outTitle, String* outMeta) {
  if (outIndex == nullptr || outTitle == nullptr || outMeta == nullptr) {
    return;
  }

  char slotIndex[4];
  snprintf(slotIndex, sizeof(slotIndex), "%02u", static_cast<unsigned>(slot + 1));
  *outIndex = slotIndex;
  *outTitle = rawLine;
  *outMeta = "Date not provided";

  const int newlinePos = rawLine.indexOf('\n');
  if (newlinePos >= 0) {
    *outTitle = rawLine.substring(0, newlinePos);
    *outMeta = rawLine.substring(newlinePos + 1);
  }

  outTitle->trim();
  outMeta->trim();

  const int dotPos = outTitle->indexOf('.');
  if (dotPos > 0 && dotPos <= 2) {
    bool allDigits = true;
    for (int i = 0; i < dotPos; ++i) {
      const char ch = (*outTitle)[i];
      if (ch < '0' || ch > '9') {
        allDigits = false;
        break;
      }
    }

    if (allDigits) {
      const String extracted = outTitle->substring(0, dotPos);
      if (extracted.length() > 0) {
        *outIndex = extracted;
      }
      *outTitle = outTitle->substring(dotPos + 1);
      outTitle->trim();
    }
  }

  if (outTitle->length() == 0) {
    *outTitle = "Untitled event";
  }
  if (outMeta->length() == 0) {
    *outMeta = "--";
  }

  if (outTitle->startsWith("{") && outTitle->indexOf("title=") >= 0) {
    const String mapSource = *outTitle;
    String extractedTitle;
    String extractedDate;
    if (parseKeyValueChunk(mapSource, "title", &extractedTitle) && extractedTitle.length() > 0) {
      *outTitle = extractedTitle;
    }
    if (parseKeyValueChunk(mapSource, "startDate", &extractedDate) && extractedDate.length() > 0) {
      *outMeta = extractedDate;
    } else if (parseKeyValueChunk(mapSource, "start", &extractedDate) && extractedDate.length() > 0) {
      *outMeta = extractedDate;
    }
  }

  if (!outMeta->startsWith("Date:")) {
    *outMeta = String("Date: ") + *outMeta;
  }
}

uint8_t estimateTitleLines(const char* titleText) {
  if (titleText == nullptr || titleText[0] == '\0') {
    return 1;
  }

  const size_t len = strlen(titleText);
  if (len <= 22) {
    return 1;
  }
  if (len <= 44) {
    return 2;
  }
  return 3;
}

uint8_t maxTitleLinesForVisibleCount(uint8_t visibleCount) {
  if (visibleCount <= 2) {
    return 3;
  }
  return 2;
}
}  // namespace

AgendaView::AgendaView(LvglHost& host, AgendaService& service)
    : host_(host),
      service_(service),
      uiReady_(false),
      needImmediateRefresh_(false),
      lastAgendaUpdateMs_(0),
      lastAgendaAttemptMs_(0),
      screen_(nullptr),
      headerCard_(nullptr),
      lblTitle_(nullptr),
      lblStatus_(nullptr),
      lblUpdated_(nullptr),
      cardItems_{nullptr, nullptr, nullptr, nullptr},
      cardAccent_{nullptr, nullptr, nullptr, nullptr},
      lblItemIndex_{nullptr, nullptr, nullptr, nullptr},
      lblItemTitle_{nullptr, nullptr, nullptr, nullptr},
      lblItemMeta_{nullptr, nullptr, nullptr, nullptr},
      lblNoAgenda_(nullptr),
      lblHint_(nullptr) {}

void AgendaView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  host_.loadScreen(screen_);
  if (lblStatus_ != nullptr) {
    lv_label_set_text(lblStatus_, "Syncing...");
    lv_obj_set_style_bg_color(lblStatus_, lv_color_hex(0x1D4ED8), LV_PART_MAIN);
    lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0xDBEAFE), LV_PART_MAIN);
  }
  if (lblNoAgenda_ != nullptr) {
    lv_obj_add_flag(lblNoAgenda_, LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    setSlotPlaceholder(i);
    if (cardItems_[i] != nullptr) {
      lv_obj_add_flag(cardItems_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (cardItems_[0] != nullptr) {
    lv_obj_clear_flag(cardItems_[0], LV_OBJ_FLAG_HIDDEN);
  }
  if (lblItemIndex_[0] != nullptr) {
    lv_label_set_text(lblItemIndex_[0], "01");
  }
  if (lblItemTitle_[0] != nullptr) {
    lv_label_set_text(lblItemTitle_[0], "Title: Loading events...");
  }
  if (lblItemMeta_[0] != nullptr) {
    lv_label_set_text(lblItemMeta_[0], "Date: syncing...");
  }
  setSlotVisual(0, 0x3B82F6, true);
  layoutVisibleSlots(1);
  if (lblUpdated_ != nullptr) {
    lv_label_set_text(lblUpdated_, "Updated: --:--");
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
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x050A14), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

  headerCard_ = lv_obj_create(screen_);
  lv_obj_set_size(headerCard_, 220, 38);
  lv_obj_align(headerCard_, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_clear_flag(headerCard_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(headerCard_, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(headerCard_, lv_color_hex(0x101827), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(headerCard_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(headerCard_, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(headerCard_, lv_color_hex(0x253247), LV_PART_MAIN);
  lv_obj_set_style_pad_left(headerCard_, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(headerCard_, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_top(headerCard_, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(headerCard_, 4, LV_PART_MAIN);

  lblTitle_ = lv_label_create(headerCard_);
  lv_label_set_text(lblTitle_, "Agenda");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 0, 0);

  lblStatus_ = lv_label_create(headerCard_);
  lv_label_set_text(lblStatus_, "Google Calendar");
  lv_obj_set_width(lblStatus_, 116);
  lv_label_set_long_mode(lblStatus_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lblStatus_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(lblStatus_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblStatus_, lv_color_hex(0xDBEAFE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(lblStatus_, lv_color_hex(0x1D4ED8), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(lblStatus_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(lblStatus_, 7, LV_PART_MAIN);
  lv_obj_set_style_pad_left(lblStatus_, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_right(lblStatus_, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_top(lblStatus_, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(lblStatus_, 2, LV_PART_MAIN);
  lv_obj_align(lblStatus_, LV_ALIGN_TOP_RIGHT, 0, 0);

  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    cardItems_[i] = lv_obj_create(screen_);
    lv_obj_set_size(cardItems_[i], 220, 46);
    lv_obj_align(cardItems_[i], LV_ALIGN_TOP_MID, 0, static_cast<lv_coord_t>(52 + (i * 50)));
    lv_obj_clear_flag(cardItems_[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(cardItems_[i], 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cardItems_[i], lv_color_hex(0x0D1626), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cardItems_[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cardItems_[i], 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cardItems_[i], lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_pad_all(cardItems_[i], 0, LV_PART_MAIN);

    cardAccent_[i] = lv_obj_create(cardItems_[i]);
    lv_obj_set_size(cardAccent_[i], 4, 30);
    lv_obj_align(cardAccent_[i], LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_clear_flag(cardAccent_[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(cardAccent_[i], 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cardAccent_[i], lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cardAccent_[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cardAccent_[i], 0, LV_PART_MAIN);

    lblItemIndex_[i] = lv_label_create(cardItems_[i]);
    lv_obj_set_style_text_font(lblItemIndex_[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblItemIndex_[i], lv_color_hex(0x64748B), LV_PART_MAIN);
    lv_obj_align(lblItemIndex_[i], LV_ALIGN_TOP_LEFT, 16, 6);

    lblItemTitle_[i] = lv_label_create(cardItems_[i]);
    lv_label_set_long_mode(lblItemTitle_[i], LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblItemTitle_[i], 174);
    lv_obj_set_style_text_font(lblItemTitle_[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblItemTitle_[i], lv_color_hex(0xF8FAFC), LV_PART_MAIN);
    lv_obj_align(lblItemTitle_[i], LV_ALIGN_TOP_LEFT, 36, 5);

    lblItemMeta_[i] = lv_label_create(cardItems_[i]);
    lv_obj_set_width(lblItemMeta_[i], 174);
    lv_label_set_long_mode(lblItemMeta_[i], LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lblItemMeta_[i], &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblItemMeta_[i], lv_color_hex(0x93A4BD), LV_PART_MAIN);
    lv_obj_align(lblItemMeta_[i], LV_ALIGN_TOP_LEFT, 36, 24);
  }

  lblNoAgenda_ = lv_label_create(screen_);
  lv_label_set_text(lblNoAgenda_, "No agenda");
  lv_obj_set_width(lblNoAgenda_, 220);
  lv_obj_set_style_text_align(lblNoAgenda_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(lblNoAgenda_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblNoAgenda_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblNoAgenda_, LV_ALIGN_TOP_MID, 0, 118);
  lv_obj_add_flag(lblNoAgenda_, LV_OBJ_FLAG_HIDDEN);

  lblUpdated_ = lv_label_create(screen_);
  lv_label_set_text(lblUpdated_, "Updated: --:--");
  lv_obj_set_style_text_font(lblUpdated_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblUpdated_, lv_color_hex(0x94A3B8), LV_PART_MAIN);
  lv_obj_align(lblUpdated_, LV_ALIGN_BOTTOM_LEFT, 10, -24);

  lblHint_ = lv_label_create(screen_);
  lv_label_set_text(lblHint_, "Touch: Next page  Hold: Settings");
  lv_obj_set_style_text_font(lblHint_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint_, lv_color_hex(0x60A5FA), LV_PART_MAIN);
  lv_obj_align(lblHint_, LV_ALIGN_BOTTOM_LEFT, 10, -8);

  uiReady_ = (headerCard_ != nullptr && lblTitle_ != nullptr && lblStatus_ != nullptr && lblNoAgenda_ != nullptr &&
              lblUpdated_ != nullptr && lblHint_ != nullptr);
  if (uiReady_) {
    for (uint8_t i = 0; i < kAgendaSlots; ++i) {
      if (cardItems_[i] == nullptr || cardAccent_[i] == nullptr || lblItemIndex_[i] == nullptr ||
          lblItemTitle_[i] == nullptr || lblItemMeta_[i] == nullptr) {
        uiReady_ = false;
        break;
      }
    }
  }

  if (uiReady_) {
    for (uint8_t i = 0; i < kAgendaSlots; ++i) {
      setSlotPlaceholder(i);
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
  lv_obj_set_style_bg_color(lblStatus_, result.success ? lv_color_hex(0x1D4ED8) : lv_color_hex(0xB91C1C), LV_PART_MAIN);
  lv_obj_set_style_text_color(lblStatus_, result.success ? lv_color_hex(0xDBEAFE) : lv_color_hex(0xFECACA), LV_PART_MAIN);
  lv_label_set_text(lblUpdated_, result.updatedText.c_str());

  if (!result.success) {
    if (lblNoAgenda_ != nullptr) {
      lv_obj_add_flag(lblNoAgenda_, LV_OBJ_FLAG_HIDDEN);
    }
    if (cardItems_[0] != nullptr) {
      lv_obj_clear_flag(cardItems_[0], LV_OBJ_FLAG_HIDDEN);
    }
    if (lblItemIndex_[0] != nullptr) {
      lv_label_set_text(lblItemIndex_[0], "!!");
    }
    if (lblItemTitle_[0] != nullptr) {
      lv_label_set_text(lblItemTitle_[0], "Title: Unable to load agenda");
    }
    if (lblItemMeta_[0] != nullptr) {
      lv_label_set_text(lblItemMeta_[0], "Date: check Wi-Fi / URL");
    }
    setSlotVisual(0, 0xEF4444, true);
    setSlotGeometry(0, 1);
    layoutVisibleSlots(1);
    for (uint8_t i = 1; i < kAgendaSlots; ++i) {
      if (cardItems_[i] != nullptr) {
        lv_obj_add_flag(cardItems_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  const uint8_t visibleCount = (result.lineCount <= kAgendaSlots) ? result.lineCount : kAgendaSlots;
  if (visibleCount == 0) {
    for (uint8_t i = 0; i < kAgendaSlots; ++i) {
      if (cardItems_[i] != nullptr) {
        lv_obj_add_flag(cardItems_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (lblNoAgenda_ != nullptr) {
      lv_label_set_text(lblNoAgenda_, "No agenda");
      lv_obj_clear_flag(lblNoAgenda_, LV_OBJ_FLAG_HIDDEN);
    }
    if (lblUpdated_ != nullptr) {
      lv_obj_set_pos(lblUpdated_, 10, 242);
    }
    lastAgendaUpdateMs_ = millis();
    return;
  }

  if (lblNoAgenda_ != nullptr) {
    lv_obj_add_flag(lblNoAgenda_, LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < kAgendaSlots; ++i) {
    if (i < visibleCount) {
      if (cardItems_[i] != nullptr) {
        lv_obj_clear_flag(cardItems_[i], LV_OBJ_FLAG_HIDDEN);
      }
      setSlotContent(i, result.lines[i]);
    } else {
      if (cardItems_[i] != nullptr) {
        lv_obj_add_flag(cardItems_[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  layoutVisibleSlots(visibleCount);

  if (result.success) {
    lastAgendaUpdateMs_ = millis();
  }
}

void AgendaView::layoutVisibleSlots(uint8_t visibleCount) {
  const uint8_t count = (visibleCount <= kAgendaSlots) ? visibleCount : kAgendaSlots;
  if (count == 0) {
    return;
  }

  const uint8_t maxLines = maxTitleLinesForVisibleCount(count);
  lv_coord_t currentY = 52;
  const lv_coord_t gap = (count <= 2) ? 6 : ((count == 3) ? 4 : 2);

  for (uint8_t i = 0; i < count; ++i) {
    uint8_t lines = 1;
    if (lblItemTitle_[i] != nullptr) {
      lines = estimateTitleLines(lv_label_get_text(lblItemTitle_[i]));
    }
    if (lines > maxLines) {
      lines = maxLines;
    }

    setSlotGeometry(i, lines);
    if (cardItems_[i] != nullptr) {
      lv_obj_align(cardItems_[i], LV_ALIGN_TOP_MID, 0, currentY);
      currentY = static_cast<lv_coord_t>(currentY + lv_obj_get_height(cardItems_[i]) + gap);
    }
  }

  if (lblUpdated_ != nullptr) {
    lv_coord_t updatedY = static_cast<lv_coord_t>(currentY + 2);
    if (updatedY > 252) {
      updatedY = 252;
    }
    lv_obj_set_pos(lblUpdated_, 10, updatedY);
  }
}

void AgendaView::setSlotGeometry(uint8_t slot, uint8_t titleLines) {
  if (slot >= kAgendaSlots) {
    return;
  }

  if (titleLines < 1) {
    titleLines = 1;
  } else if (titleLines > 3) {
    titleLines = 3;
  }

  const lv_coord_t dateY = 5;
  const lv_coord_t titleY = 24;
  const lv_coord_t titleHeight = static_cast<lv_coord_t>(titleLines * 14);
  const lv_coord_t cardHeight = static_cast<lv_coord_t>(titleY + titleHeight + 4);

  if (lblItemTitle_[slot] != nullptr) {
    lv_obj_set_size(lblItemTitle_[slot], 174, titleHeight);
    lv_obj_align(lblItemTitle_[slot], LV_ALIGN_TOP_LEFT, 36, titleY);
  }
  if (lblItemMeta_[slot] != nullptr) {
    lv_obj_align(lblItemMeta_[slot], LV_ALIGN_TOP_LEFT, 36, dateY);
  }
  if (cardItems_[slot] != nullptr) {
    lv_obj_set_height(cardItems_[slot], (cardHeight < 46) ? 46 : cardHeight);
  }
  if (cardAccent_[slot] != nullptr && cardItems_[slot] != nullptr) {
    const lv_coord_t accentHeight = static_cast<lv_coord_t>(lv_obj_get_height(cardItems_[slot]) - 16);
    lv_obj_set_size(cardAccent_[slot], 4, (accentHeight < 22) ? 22 : accentHeight);
    lv_obj_align(cardAccent_[slot], LV_ALIGN_LEFT_MID, 8, 0);
  }
}

void AgendaView::setSlotPlaceholder(uint8_t slot) {
  if (slot >= kAgendaSlots) {
    return;
  }

  char indexText[4];
  snprintf(indexText, sizeof(indexText), "%02u", static_cast<unsigned>(slot + 1));
  if (lblItemIndex_[slot] != nullptr) {
    lv_label_set_text(lblItemIndex_[slot], indexText);
  }
  if (lblItemTitle_[slot] != nullptr) {
    lv_label_set_text(lblItemTitle_[slot], "Title: No scheduled event");
  }
  if (lblItemMeta_[slot] != nullptr) {
    lv_label_set_text(lblItemMeta_[slot], "Date: waiting");
  }
  setSlotGeometry(slot, 1);
  setSlotVisual(slot, 0x334155, false);
}

void AgendaView::setSlotContent(uint8_t slot, const String& rawLine) {
  if (slot >= kAgendaSlots) {
    return;
  }

  String indexText;
  String titleText;
  String metaText;
  parseAgendaLine(rawLine, slot, &indexText, &titleText, &metaText);

  if (lblItemIndex_[slot] != nullptr) {
    lv_label_set_text(lblItemIndex_[slot], indexText.c_str());
  }
  if (lblItemTitle_[slot] != nullptr) {
    lv_label_set_text_fmt(lblItemTitle_[slot], "Title: %s", titleText.c_str());
  }
  if (lblItemMeta_[slot] != nullptr) {
    lv_label_set_text(lblItemMeta_[slot], metaText.c_str());
  }

  String displayTitle = String("Title: ") + titleText;
  uint8_t titleLines = estimateTitleLines(displayTitle.c_str());
  if (titleLines > 3) {
    titleLines = 3;
  }
  setSlotGeometry(slot, titleLines);

  static const uint32_t kSlotAccentHex[kAgendaSlots] = {0x3B82F6, 0x0EA5E9, 0x14B8A6, 0x22C55E};
  setSlotVisual(slot, kSlotAccentHex[slot], true);
}

void AgendaView::setSlotVisual(uint8_t slot, uint32_t accentHex, bool active) {
  if (slot >= kAgendaSlots) {
    return;
  }

  const uint32_t bgHex = active ? 0x101B2F : 0x0B1321;
  const uint32_t borderHex = active ? 0x31415C : 0x1E293B;
  const uint32_t titleHex = active ? 0xF8FAFC : 0x64748B;
  const uint32_t metaHex = active ? 0x9FB1CA : 0x475569;

  if (cardItems_[slot] != nullptr) {
    lv_obj_set_style_bg_color(cardItems_[slot], lv_color_hex(bgHex), LV_PART_MAIN);
    lv_obj_set_style_border_color(cardItems_[slot], lv_color_hex(borderHex), LV_PART_MAIN);
  }
  if (cardAccent_[slot] != nullptr) {
    lv_obj_set_style_bg_color(cardAccent_[slot], lv_color_hex(accentHex), LV_PART_MAIN);
  }
  if (lblItemIndex_[slot] != nullptr) {
    lv_obj_set_style_text_color(lblItemIndex_[slot], lv_color_hex(accentHex), LV_PART_MAIN);
  }
  if (lblItemTitle_[slot] != nullptr) {
    lv_obj_set_style_text_color(lblItemTitle_[slot], lv_color_hex(titleHex), LV_PART_MAIN);
  }
  if (lblItemMeta_[slot] != nullptr) {
    lv_obj_set_style_text_color(lblItemMeta_[slot], lv_color_hex(metaHex), LV_PART_MAIN);
  }
}
