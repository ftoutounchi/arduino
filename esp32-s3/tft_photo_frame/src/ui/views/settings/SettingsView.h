#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "config/AppConfig.h"
#include "infra/display/DisplayRenderer.h"
#include "ui/LvglHost.h"
#include "ui/views/IView.h"

class SettingsView : public IView {
 public:
  SettingsView(LvglHost& host, DisplayRenderer& display);

  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  ViewAction handleEvent(const PageEvent& event) override;

 private:
  enum class TabId : uint8_t {
    kAuto = 0,
    kPhoto = 1,
    kWeather = 2,
    kSystem = 3,
  };

  enum class SettingItem : uint8_t {
    kTabSwitch,
    kTimeoutEnabled,
    kTimeoutSeconds,
    kCycleEnabled,
    kCyclePhotoCount,
    kCycleDuration,
    kPhotoSeconds,
    kPhotoMode,
    kDisplayTransform,
    kWeatherPreset,
    kWeatherLatitude,
    kWeatherLongitude,
    kAlarmSummary,
    kClientIp,
    kExit,
  };

  static constexpr uint8_t kTabCount = 4;
  static constexpr uint8_t kRowCount = 6;

  LvglHost& host_;
  DisplayRenderer& display_;

  bool uiReady_;
  lv_obj_t* screen_;
  lv_obj_t* lblTitle_;
  lv_obj_t* tabBoxes_[kTabCount];
  lv_obj_t* tabLabels_[kTabCount];
  lv_obj_t* lblRows_[kRowCount];
  lv_obj_t* lblHint1_;
  lv_obj_t* lblHint2_;
  uint8_t activeTabIndex_;
  uint8_t selectedRowIndex_;

  void ensureUi();
  void createUi();
  void refreshUi();
  const char* tabName(uint8_t tabIndex) const;
  uint8_t rowCountForTab(uint8_t tabIndex) const;
  SettingItem itemForRow(uint8_t tabIndex, uint8_t rowIndex) const;
  void formatRowText(SettingItem item, char* out, size_t outSize) const;
  bool applyEdit(SettingItem item);
  void setWeatherPreset(uint8_t presetIndex);
  uint8_t weatherPresetIndex() const;
};
