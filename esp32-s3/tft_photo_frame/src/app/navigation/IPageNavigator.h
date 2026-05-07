#pragma once

#include "app/navigation/IPage.h"

class IPageNavigator {
 public:
  virtual ~IPageNavigator() = default;

  virtual bool requestSwitch(IPage::Id pageId) = 0;
  virtual bool requestPush(IPage::Id pageId) = 0;
  virtual bool requestPop() = 0;
};
