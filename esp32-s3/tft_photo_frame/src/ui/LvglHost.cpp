#include "ui/LvglHost.h"

#if LV_USE_PNG
extern "C" void lv_png_init(void);
#endif

LvglHost::LvglHost(DisplayRenderer& display)
    : displayRenderer_(display),
      ready_(false),
      lastTickMs_(0),
      drawBuf_(),
      dispDrv_(),
      display_(nullptr),
      drawBuffer_{} {}

void LvglHost::begin() {
  if (ready_) {
    return;
  }

  lv_init();
#if LV_USE_PNG
  lv_png_init();
#endif

  lv_disp_draw_buf_init(&drawBuf_, drawBuffer_, nullptr, Config::kDisplayWidth * kDrawBufferLines);
  lv_disp_drv_init(&dispDrv_);
  dispDrv_.hor_res = Config::kDisplayWidth;
  dispDrv_.ver_res = Config::kDisplayHeight;
  dispDrv_.flush_cb = &LvglHost::flushCallback;
  dispDrv_.draw_buf = &drawBuf_;
  dispDrv_.user_data = this;
  display_ = lv_disp_drv_register(&dispDrv_);

  ready_ = true;
  lastTickMs_ = millis();
}

bool LvglHost::isReady() const {
  return ready_;
}

void LvglHost::loadScreen(lv_obj_t* screen) {
  if (!ready_ || screen == nullptr) {
    return;
  }
  lv_disp_set_default(display_);
  lv_scr_load(screen);
}

void LvglHost::service() {
  if (!ready_) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = now - lastTickMs_;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastTickMs_ = now;
  }
  lv_timer_handler();
}

lv_disp_t* LvglHost::display() const {
  return display_;
}

void LvglHost::flushCallback(lv_disp_drv_t* dispDrv, const lv_area_t* area, lv_color_t* colorP) {
  LvglHost* self = static_cast<LvglHost*>(dispDrv->user_data);
  if (self != nullptr) {
    self->flushDisplay(area, colorP);
  }
  lv_disp_flush_ready(dispDrv);
}

void LvglHost::flushDisplay(const lv_area_t* area, lv_color_t* colorP) {
  if (area == nullptr || colorP == nullptr) {
    return;
  }

  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  if (width <= 0 || height <= 0) {
    return;
  }

  displayRenderer_.drawRgbBitmap(area->x1, area->y1, reinterpret_cast<const uint16_t*>(colorP),
                                 static_cast<uint16_t>(width), static_cast<uint16_t>(height));
}
