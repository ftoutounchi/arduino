#pragma once

#include "ui/LvglHost.h"
#include "ui/views/agenda/AgendaView.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"
#include "app/services/AgendaService.h"

class AgendaPage : public IPage {
 public:
  AgendaPage(LvglHost& host, AgendaService& agendaService, IPageNavigator& navigator);

  Id id() const override { return Id::kAgenda; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

 private:
  AgendaView view_;
  IPageNavigator& navigator_;
};
