#pragma once

#include "infra/display/DisplayRenderer.h"
#include "ui/LvglHost.h"
#include "ui/views/settings/SettingsView.h"
#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"

class SettingsPage : public IPage {
 public:
  SettingsPage(LvglHost& host, DisplayRenderer& display, IPageNavigator& navigator);

  Id id() const override { return Id::kSettings; }
  void onEnter() override;
  void onExit() override;
  void update(uint32_t nowMs) override;
  void render() override;
  void handleEvent(const PageEvent& event) override;

 private:
  SettingsView view_;
  IPageNavigator& navigator_;
};
