#pragma once

#include <Arduino.h>

#include "infra/display/DisplayRenderer.h"
#include "infra/network/JpegDownloader.h"
#include "infra/network/GoogleScriptAgendaProvider.h"
#include "infra/network/OpenMeteoWeatherProvider.h"
#include "ui/LvglHost.h"
#include "input/BootButtonInput.h"
#include "app/pages/AgendaPage.h"
#include "app/pages/AlarmPage.h"
#include "app/pages/CalendarPage.h"
#include "app/pages/DashboardPage.h"
#include "app/navigation/PageManager.h"
#include "app/pages/PhotoFramePage.h"
#include "app/pages/SettingsPage.h"
#include "app/services/AgendaService.h"
#include "app/services/AlarmScheduler.h"
#include "app/services/AutoViewState.h"
#include "app/services/PhotoDownloadService.h"
#include "app/services/WeatherService.h"
#include "infra/web/ConfigWebServer.h"
#include "infra/wifi/WifiManager.h"

class PhotoFrameApp {
 public:
  PhotoFrameApp();

  void begin();
  void loop();

 private:
  DisplayRenderer display_;
  WifiManager wifi_;
  JpegDownloader downloader_;
  PhotoDownloadService photoDownloads_;
  GoogleScriptAgendaProvider agendaProvider_;
  OpenMeteoWeatherProvider weatherProvider_;
  AgendaService agendaService_;
  AlarmScheduler alarmScheduler_;
  WeatherService weatherService_;
  AutoViewState autoViewState_;
  LvglHost lvglHost_;
  PageManager pageManager_;
  PhotoFramePage photoFramePage_;
  DashboardPage dashboardPage_;
  CalendarPage calendarPage_;
  AgendaPage agendaPage_;
  AlarmPage alarmPage_;
  SettingsPage settingsPage_;
  BootButtonInput bootButton_;
  ConfigWebServer configWeb_;

  bool configReloadPending_;

  static void onWebConfigChanged(void* ctx);
  void registerPages();
  void requestConfigReload();
  void processConfigReload();
};
