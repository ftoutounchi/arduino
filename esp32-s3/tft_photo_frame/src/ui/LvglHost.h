#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "config/AppConfig.h"
#include "infra/display/DisplayRenderer.h"

class LvglHost {
 public:
  explicit LvglHost(DisplayRenderer& display);

  void begin();
  bool isReady() const;

  void loadScreen(lv_obj_t* screen);
  void service();
  lv_disp_t* display() const;

 private:
  static constexpr uint16_t kDrawBufferLines = 20;

  DisplayRenderer& displayRenderer_;
  bool ready_;
  uint32_t lastTickMs_;

  lv_disp_draw_buf_t drawBuf_;
  lv_disp_drv_t dispDrv_;
  lv_disp_t* display_;
  lv_color_t drawBuffer_[Config::kDisplayWidth * kDrawBufferLines];

  static void flushCallback(lv_disp_drv_t* dispDrv, const lv_area_t* area, lv_color_t* colorP);
  void flushDisplay(const lv_area_t* area, lv_color_t* colorP);
};
