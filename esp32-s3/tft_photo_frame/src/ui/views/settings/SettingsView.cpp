#include "ui/views/settings/SettingsView.h"

#include <WiFi.h>

#include <math.h>
#include <string.h>

namespace {
struct WeatherPreset {
  const char* label;
  double latitude;
  double longitude;
};

constexpr WeatherPreset kWeatherPresets[] = {
    {"Hamburg", 53.6060, 10.0676},
    {"Berlin", 52.5200, 13.4050},
    {"Munich", 48.1374, 11.5755},
    {"Frankfurt", 50.1109, 8.6821},
    {"New York", 40.7128, -74.0060},
    {"Tokyo", 35.6762, 139.6503},
};
constexpr uint8_t kWeatherPresetCount = static_cast<uint8_t>(sizeof(kWeatherPresets) / sizeof(kWeatherPresets[0]));
const char kCustomWeatherLabel[] = "Custom";

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

bool nearlyEqual(double a, double b) {
  return fabs(a - b) <= 0.0005;
}

void formatCyclePagesMask(uint8_t mask, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return;
  }

  out[0] = '\0';
  bool first = true;
  const auto appendLabel = [&](const char* label) {
    if (label == nullptr || label[0] == '\0') {
      return;
    }
    if (!first) {
      strncat(out, "|", outSize - strlen(out) - 1);
    }
    strncat(out, label, outSize - strlen(out) - 1);
    first = false;
  };

  if ((mask & Config::kAutoCyclePageDashboard) != 0) {
    appendLabel("Dash");
  }
  if ((mask & Config::kAutoCyclePageCalendar) != 0) {
    appendLabel("Cal");
  }
  if ((mask & Config::kAutoCyclePageAgenda) != 0) {
    appendLabel("Agenda");
  }
  if ((mask & Config::kAutoCyclePageAlarm) != 0) {
    appendLabel("Alarm");
  }

  if (out[0] == '\0') {
    copyCString(out, outSize, "Dash");
  }
}
}  // namespace

SettingsView::SettingsView(LvglHost& host, DisplayRenderer& display)
    : host_(host),
      display_(display),
      uiReady_(false),
      screen_(nullptr),
      lblTitle_(nullptr),
      tabBoxes_{nullptr, nullptr, nullptr, nullptr},
      tabLabels_{nullptr, nullptr, nullptr, nullptr},
      lblRows_{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
      lblHint1_(nullptr),
      lblHint2_(nullptr),
      activeTabIndex_(static_cast<uint8_t>(TabId::kAuto)),
      selectedRowIndex_(0) {}

void SettingsView::onEnter() {
  ensureUi();
  if (!uiReady_ || screen_ == nullptr) {
    return;
  }

  const uint8_t rows = rowCountForTab(activeTabIndex_);
  if (selectedRowIndex_ >= rows) {
    selectedRowIndex_ = 0;
  }

  refreshUi();
  host_.loadScreen(screen_);
}

void SettingsView::onExit() {}

void SettingsView::update(uint32_t nowMs) {
  (void)nowMs;
}

void SettingsView::render() {
  host_.service();
}

ViewAction SettingsView::handleEvent(const PageEvent& event) {
  if (event.type == PageEvent::Type::kBootShortPress) {
    const uint8_t rows = rowCountForTab(activeTabIndex_);
    if (rows == 0) {
      return ViewAction::kNone;
    }
    selectedRowIndex_ = static_cast<uint8_t>((selectedRowIndex_ + 1U) % rows);
    refreshUi();
    return ViewAction::kNone;
  }

  if (event.type == PageEvent::Type::kBootLongPress) {
    const SettingItem item = itemForRow(activeTabIndex_, selectedRowIndex_);
    if (item == SettingItem::kExit) {
      return ViewAction::kCloseView;
    }

    const bool changed = applyEdit(item);
    if (changed) {
      Config::saveAutoViewSettings();
    }
    refreshUi();
  }

  return ViewAction::kNone;
}

void SettingsView::ensureUi() {
  if (uiReady_) {
    return;
  }
  createUi();
}

void SettingsView::createUi() {
  if (screen_ != nullptr) {
    return;
  }

  screen_ = lv_obj_create(nullptr);
  if (screen_ == nullptr) {
    return;
  }

  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x060B18), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

  lblTitle_ = lv_label_create(screen_);
  lv_label_set_text(lblTitle_, "Settings [AUTO]");
  lv_obj_set_style_text_font(lblTitle_, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblTitle_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(lblTitle_, LV_ALIGN_TOP_LEFT, 10, 8);

  for (uint8_t i = 0; i < kTabCount; ++i) {
    tabBoxes_[i] = lv_obj_create(screen_);
    tabLabels_[i] = lv_label_create(tabBoxes_[i]);

    lv_obj_set_size(tabBoxes_[i], 52, 22);
    lv_obj_align(tabBoxes_[i], LV_ALIGN_TOP_LEFT, static_cast<lv_coord_t>(8 + (i * 56)), 38);
    lv_obj_clear_flag(tabBoxes_[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tabBoxes_[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(tabBoxes_[i], 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(tabBoxes_[i], 1, LV_PART_MAIN);

    lv_label_set_text(tabLabels_[i], tabName(i));
    lv_obj_set_style_text_font(tabLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(tabLabels_[i]);
  }

  for (uint8_t i = 0; i < kRowCount; ++i) {
    lblRows_[i] = lv_label_create(screen_);
    lv_obj_set_style_text_font(lblRows_[i], &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblRows_[i], lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_width(lblRows_[i], 220);
    lv_label_set_long_mode(lblRows_[i], LV_LABEL_LONG_DOT);
    lv_obj_align(lblRows_[i], LV_ALIGN_TOP_LEFT, 10, static_cast<int16_t>(68 + (i * 22)));
  }

  lblHint1_ = lv_label_create(screen_);
  lv_label_set_text(lblHint1_, "Tap TOUCH: Next row");
  lv_obj_set_style_text_font(lblHint1_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint1_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint1_, LV_ALIGN_BOTTOM_LEFT, 10, -28);

  lblHint2_ = lv_label_create(screen_);
  lv_label_set_text(lblHint2_, "Hold TOUCH: Edit / Tab / Exit");
  lv_obj_set_style_text_font(lblHint2_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(lblHint2_, lv_color_hex(0x93C5FD), LV_PART_MAIN);
  lv_obj_align(lblHint2_, LV_ALIGN_BOTTOM_LEFT, 10, -8);

  uiReady_ = (lblTitle_ != nullptr && lblHint1_ != nullptr && lblHint2_ != nullptr);
  if (uiReady_) {
    for (uint8_t i = 0; i < kTabCount; ++i) {
      if (tabBoxes_[i] == nullptr || tabLabels_[i] == nullptr) {
        uiReady_ = false;
        break;
      }
    }
  }
  if (uiReady_) {
    for (uint8_t i = 0; i < kRowCount; ++i) {
      if (lblRows_[i] == nullptr) {
        uiReady_ = false;
        break;
      }
    }
  }

  refreshUi();
}

void SettingsView::refreshUi() {
  if (!uiReady_) {
    return;
  }

  char title[32];
  snprintf(title, sizeof(title), "Settings [%s]", tabName(activeTabIndex_));
  lv_label_set_text(lblTitle_, title);

  for (uint8_t i = 0; i < kTabCount; ++i) {
    const bool active = (i == activeTabIndex_);
    lv_obj_set_style_bg_color(tabBoxes_[i], active ? lv_color_hex(0x2563EB) : lv_color_hex(0x0F1A2F), LV_PART_MAIN);
    lv_obj_set_style_border_color(tabBoxes_[i], active ? lv_color_hex(0x3B82F6) : lv_color_hex(0x30405F), LV_PART_MAIN);
    lv_obj_set_style_text_color(tabLabels_[i], active ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x93A5C4), LV_PART_MAIN);
  }

  const uint8_t rows = rowCountForTab(activeTabIndex_);
  if (rows == 0) {
    selectedRowIndex_ = 0;
  } else if (selectedRowIndex_ >= rows) {
    selectedRowIndex_ = 0;
  }

  for (uint8_t i = 0; i < kRowCount; ++i) {
    if (i >= rows) {
      lv_obj_add_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    lv_obj_clear_flag(lblRows_[i], LV_OBJ_FLAG_HIDDEN);

    const SettingItem item = itemForRow(activeTabIndex_, i);
    char text[96];
    formatRowText(item, text, sizeof(text));

    const bool selected = (i == selectedRowIndex_);
    char finalText[100];
    snprintf(finalText, sizeof(finalText), "%c %s", selected ? '>' : ' ', text);
    lv_label_set_text(lblRows_[i], finalText);
    lv_obj_set_style_text_color(lblRows_[i], selected ? lv_color_hex(0xF59E0B) : lv_color_hex(0xD1D5DB),
                                LV_PART_MAIN);
  }
}

const char* SettingsView::tabName(uint8_t tabIndex) const {
  switch (static_cast<TabId>(tabIndex)) {
    case TabId::kAuto:
      return "AUTO";
    case TabId::kPhoto:
      return "PHOTO";
    case TabId::kWeather:
      return "WEATHER";
    case TabId::kSystem:
      return "SYSTEM";
    default:
      return "AUTO";
  }
}

uint8_t SettingsView::rowCountForTab(uint8_t tabIndex) const {
  switch (static_cast<TabId>(tabIndex)) {
    case TabId::kAuto:
      return 7;
    case TabId::kPhoto:
      return 4;
    case TabId::kWeather:
      return 4;
    case TabId::kSystem:
      return 4;
    default:
      return 4;
  }
}

SettingsView::SettingItem SettingsView::itemForRow(uint8_t tabIndex, uint8_t rowIndex) const {
  switch (static_cast<TabId>(tabIndex)) {
    case TabId::kAuto:
      switch (rowIndex) {
        case 0:
          return SettingItem::kTabSwitch;
        case 1:
          return SettingItem::kTimeoutEnabled;
        case 2:
          return SettingItem::kTimeoutSeconds;
        case 3:
          return SettingItem::kCycleEnabled;
        case 4:
          return SettingItem::kCyclePages;
        case 5:
          return SettingItem::kCyclePhotoCount;
        case 6:
          return SettingItem::kCycleDuration;
        default:
          return SettingItem::kTabSwitch;
      }
    case TabId::kPhoto:
      switch (rowIndex) {
        case 0:
          return SettingItem::kTabSwitch;
        case 1:
          return SettingItem::kPhotoSeconds;
        case 2:
          return SettingItem::kPhotoMode;
        case 3:
          return SettingItem::kDisplayTransform;
        default:
          return SettingItem::kTabSwitch;
      }
    case TabId::kWeather:
      switch (rowIndex) {
        case 0:
          return SettingItem::kTabSwitch;
        case 1:
          return SettingItem::kWeatherPreset;
        case 2:
          return SettingItem::kWeatherLatitude;
        case 3:
          return SettingItem::kWeatherLongitude;
        default:
          return SettingItem::kTabSwitch;
      }
    case TabId::kSystem:
      switch (rowIndex) {
        case 0:
          return SettingItem::kTabSwitch;
        case 1:
          return SettingItem::kAlarmSummary;
        case 2:
          return SettingItem::kClientIp;
        case 3:
          return SettingItem::kExit;
        default:
          return SettingItem::kTabSwitch;
      }
    default:
      return SettingItem::kTabSwitch;
  }
}

void SettingsView::formatRowText(SettingItem item, char* out, size_t outSize) const {
  if (out == nullptr || outSize == 0) {
    return;
  }

  const Config::AutoViewSettings& settings = Config::gAutoViewSettings;
  switch (item) {
    case SettingItem::kTabSwitch:
      snprintf(out, outSize, "Tab: %s (hold)", tabName(activeTabIndex_));
      break;
    case SettingItem::kTimeoutEnabled:
      snprintf(out, outSize, "Auto timeout: %s", settings.infoPageAutoTimeoutEnabled ? "ON" : "OFF");
      break;
    case SettingItem::kTimeoutSeconds:
      snprintf(out, outSize, "Timeout sec: %lu", static_cast<unsigned long>(settings.infoPageAutoTimeoutMs / 1000));
      break;
    case SettingItem::kCycleEnabled:
      snprintf(out, outSize, "Auto cycle: %s", settings.infoPageAutoCycleEnabled ? "ON" : "OFF");
      break;
    case SettingItem::kCyclePages: {
      char pages[32];
      formatCyclePagesMask(settings.infoPageAutoCyclePagesMask, pages, sizeof(pages));
      snprintf(out, outSize, "Cycle pages: %s", pages);
      break;
    }
    case SettingItem::kCyclePhotoCount:
      snprintf(out, outSize, "Cycle photos: %u", static_cast<unsigned>(settings.infoPageAutoCyclePhotoCount));
      break;
    case SettingItem::kCycleDuration:
      snprintf(out, outSize, "Cycle sec: %lu", static_cast<unsigned long>(settings.infoPageAutoCycleDurationMs / 1000));
      break;
    case SettingItem::kPhotoSeconds:
      snprintf(out, outSize, "Photo sec: %lu", static_cast<unsigned long>(settings.photoRefreshIntervalMs / 1000));
      break;
    case SettingItem::kPhotoMode:
      snprintf(out, outSize, "Photo mode: %s", settings.photoFillMode ? "Fill" : "Fit");
      break;
    case SettingItem::kDisplayTransform: {
      const uint8_t transform =
          (Config::gDisplayTransform <= Config::kDisplayTransformMax) ? Config::gDisplayTransform
                                                                       : Config::kDefaultDisplayTransform;
      const char* transformLabel = "Normal";
      switch (transform) {
        case 1:
          transformLabel = "Mirror X";
          break;
        case 2:
          transformLabel = "Mirror Y";
          break;
        case 3:
          transformLabel = "Rotate 180";
          break;
        default:
          break;
      }
      snprintf(out, outSize, "Display: %s", transformLabel);
      break;
    }
    case SettingItem::kWeatherPreset:
      snprintf(out, outSize, "Location: %s", Config::gWeatherLocationLabel);
      break;
    case SettingItem::kWeatherLatitude:
      snprintf(out, outSize, "Latitude: %.4f", Config::gWeatherLatitude);
      break;
    case SettingItem::kWeatherLongitude:
      snprintf(out, outSize, "Longitude: %.4f", Config::gWeatherLongitude);
      break;
    case SettingItem::kAlarmSummary: {
      uint8_t activeAlarms = 0;
      for (uint8_t i = 0; i < Config::gAlarmCount; ++i) {
        if (Config::gAlarms[i].enabled) {
          ++activeAlarms;
        }
      }
      snprintf(out, outSize, "Alarms: %u / %u active", static_cast<unsigned>(activeAlarms),
               static_cast<unsigned>(Config::gAlarmCount));
      break;
    }
    case SettingItem::kClientIp:
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(out, outSize, "IP: %u.%u.%u.%u", static_cast<unsigned>(ip[0]), static_cast<unsigned>(ip[1]),
                 static_cast<unsigned>(ip[2]), static_cast<unsigned>(ip[3]));
      } else {
        snprintf(out, outSize, "IP: offline");
      }
      break;
    case SettingItem::kExit:
      snprintf(out, outSize, "Exit to Photo Frame");
      break;
    default:
      out[0] = '\0';
      break;
  }
}

bool SettingsView::applyEdit(SettingItem item) {
  Config::AutoViewSettings& settings = Config::gAutoViewSettings;

  switch (item) {
    case SettingItem::kTabSwitch:
      activeTabIndex_ = static_cast<uint8_t>((activeTabIndex_ + 1U) % kTabCount);
      selectedRowIndex_ = 0;
      return false;
    case SettingItem::kTimeoutEnabled:
      settings.infoPageAutoTimeoutEnabled = !settings.infoPageAutoTimeoutEnabled;
      return true;
    case SettingItem::kTimeoutSeconds: {
      uint32_t seconds = settings.infoPageAutoTimeoutMs / 1000;
      seconds += 5;
      if (seconds > 300) {
        seconds = 5;
      }
      settings.infoPageAutoTimeoutMs = seconds * 1000;
      return true;
    }
    case SettingItem::kCycleEnabled:
      settings.infoPageAutoCycleEnabled = !settings.infoPageAutoCycleEnabled;
      return true;
    case SettingItem::kCyclePages: {
      uint8_t nextMask = static_cast<uint8_t>((settings.infoPageAutoCyclePagesMask + 1U) &
                                              Config::kAutoCyclePageMaskAll);
      if (nextMask == 0) {
        nextMask = Config::kDefaultInfoPageAutoCyclePagesMask;
      }
      settings.infoPageAutoCyclePagesMask = nextMask;
      return true;
    }
    case SettingItem::kCyclePhotoCount:
      settings.infoPageAutoCyclePhotoCount =
          static_cast<uint16_t>((settings.infoPageAutoCyclePhotoCount >= 20) ? 1 : (settings.infoPageAutoCyclePhotoCount + 1));
      return true;
    case SettingItem::kCycleDuration: {
      uint32_t seconds = settings.infoPageAutoCycleDurationMs / 1000;
      seconds += 5;
      if (seconds > 300) {
        seconds = 5;
      }
      settings.infoPageAutoCycleDurationMs = seconds * 1000;
      return true;
    }
    case SettingItem::kPhotoSeconds: {
      uint32_t seconds = settings.photoRefreshIntervalMs / 1000;
      seconds += 5;
      if (seconds > 300) {
        seconds = 5;
      }
      settings.photoRefreshIntervalMs = seconds * 1000;
      return true;
    }
    case SettingItem::kPhotoMode:
      settings.photoFillMode = !settings.photoFillMode;
      return true;
    case SettingItem::kDisplayTransform:
      Config::gDisplayTransform =
          static_cast<uint8_t>((Config::gDisplayTransform + 1U) % (Config::kDisplayTransformMax + 1U));
      display_.setDisplayTransform(Config::gDisplayTransform);
      return true;
    case SettingItem::kWeatherPreset: {
      const uint8_t current = weatherPresetIndex();
      const uint8_t next = (current >= kWeatherPresetCount) ? 0 : static_cast<uint8_t>((current + 1U) % kWeatherPresetCount);
      setWeatherPreset(next);
      return true;
    }
    case SettingItem::kWeatherLatitude:
      Config::gWeatherLatitude += 0.25;
      if (Config::gWeatherLatitude > 90.0) {
        Config::gWeatherLatitude = -90.0;
      }
      if (weatherPresetIndex() < kWeatherPresetCount) {
        setWeatherPreset(weatherPresetIndex());
      } else {
        copyCString(Config::gWeatherLocationLabel, sizeof(Config::gWeatherLocationLabel), kCustomWeatherLabel);
      }
      return true;
    case SettingItem::kWeatherLongitude:
      Config::gWeatherLongitude += 0.25;
      if (Config::gWeatherLongitude > 180.0) {
        Config::gWeatherLongitude = -180.0;
      }
      if (weatherPresetIndex() < kWeatherPresetCount) {
        setWeatherPreset(weatherPresetIndex());
      } else {
        copyCString(Config::gWeatherLocationLabel, sizeof(Config::gWeatherLocationLabel), kCustomWeatherLabel);
      }
      return true;
    case SettingItem::kAlarmSummary:
    case SettingItem::kClientIp:
    case SettingItem::kExit:
      return false;
    default:
      return false;
  }
}

void SettingsView::setWeatherPreset(uint8_t presetIndex) {
  if (presetIndex >= kWeatherPresetCount) {
    return;
  }
  Config::gWeatherLatitude = kWeatherPresets[presetIndex].latitude;
  Config::gWeatherLongitude = kWeatherPresets[presetIndex].longitude;
  copyCString(Config::gWeatherLocationLabel, sizeof(Config::gWeatherLocationLabel), kWeatherPresets[presetIndex].label);
}

uint8_t SettingsView::weatherPresetIndex() const {
  for (uint8_t i = 0; i < kWeatherPresetCount; ++i) {
    if (nearlyEqual(Config::gWeatherLatitude, kWeatherPresets[i].latitude) &&
        nearlyEqual(Config::gWeatherLongitude, kWeatherPresets[i].longitude)) {
      return i;
    }
  }

  for (uint8_t i = 0; i < kWeatherPresetCount; ++i) {
    if (strncmp(Config::gWeatherLocationLabel, kWeatherPresets[i].label, Config::kMaxWeatherLabelLength) == 0) {
      return i;
    }
  }

  return kWeatherPresetCount;
}
