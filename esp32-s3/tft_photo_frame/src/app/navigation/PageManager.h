#pragma once

#include <stddef.h>

#include "app/navigation/IPage.h"
#include "app/navigation/IPageNavigator.h"

class PageManager : public IPageNavigator {
 public:
  static constexpr size_t kMaxDepth = 8;

  PageManager();

  bool registerPage(IPage& page);
  bool start(IPage::Id rootPageId);

  void handleEvent(const PageEvent& event);
  void update(uint32_t nowMs);
  void render();

  IPage* currentPage();
  const IPage* currentPage() const;
  size_t depth() const;

  bool requestSwitch(IPage::Id pageId) override;
  bool requestPush(IPage::Id pageId) override;
  bool requestPop() override;

 private:
  enum class PendingAction : uint8_t { kNone, kSwitch, kPush, kPop };

  IPage* registry_[static_cast<size_t>(IPage::Id::kCount)];
  IPage* stack_[kMaxDepth];
  size_t depth_;

  PendingAction pendingAction_;
  IPage::Id pendingTarget_;

  bool applyPendingAction();
  bool switchTo(IPage& next);
  bool push(IPage& next);
  bool pop();
};
