#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "app/services/AgendaService.h"
#include "config/AppConfig.h"
#include "ui/LvglHost.h"
#include "ui/views/IView.h"

class AgendaView : public IView {
 public:
  AgendaView(LvglHost& host, AgendaService& service);

  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  ViewAction handleEvent(const PageEvent& event) override;

 private:
  static constexpr uint8_t kAgendaSlots = 4;

  LvglHost& host_;
  AgendaService& service_;

  bool uiReady_;
  bool needImmediateRefresh_;
  uint32_t lastAgendaUpdateMs_;
  uint32_t lastAgendaAttemptMs_;

  lv_obj_t* screen_;
  lv_obj_t* headerCard_;
  lv_obj_t* lblTitle_;
  lv_obj_t* lblStatus_;
  lv_obj_t* lblUpdated_;
  lv_obj_t* cardItems_[kAgendaSlots];
  lv_obj_t* cardAccent_[kAgendaSlots];
  lv_obj_t* lblItemIndex_[kAgendaSlots];
  lv_obj_t* lblItemTitle_[kAgendaSlots];
  lv_obj_t* lblItemMeta_[kAgendaSlots];
  lv_obj_t* lblNoAgenda_;
  lv_obj_t* lblHint_;

  void ensureUi();
  void createUi();
  void updateAgendaItems();
  void layoutVisibleSlots(uint8_t visibleCount);
  void setSlotGeometry(uint8_t slot, uint8_t titleLines);
  void setSlotPlaceholder(uint8_t slot);
  void setSlotContent(uint8_t slot, const String& rawLine);
  void setSlotVisual(uint8_t slot, uint32_t accentHex, bool active);
};
