#include "app/navigation/PageManager.h"

PageManager::PageManager()
    : registry_{nullptr},
      stack_{nullptr},
      depth_(0),
      pendingAction_(PendingAction::kNone),
      pendingTarget_(IPage::Id::kPhotoFrame) {}

bool PageManager::registerPage(IPage& page) {
  const size_t index = static_cast<size_t>(page.id());
  if (index >= static_cast<size_t>(IPage::Id::kCount)) {
    return false;
  }
  registry_[index] = &page;
  return true;
}

bool PageManager::start(IPage::Id rootPageId) {
  const size_t index = static_cast<size_t>(rootPageId);
  if (index >= static_cast<size_t>(IPage::Id::kCount) || registry_[index] == nullptr) {
    return false;
  }

  while (depth_ > 0) {
    IPage* top = stack_[depth_ - 1];
    if (top != nullptr) {
      top->onExit();
    }
    stack_[depth_ - 1] = nullptr;
    --depth_;
  }

  stack_[0] = registry_[index];
  depth_ = 1;
  stack_[0]->onEnter();
  pendingAction_ = PendingAction::kNone;
  return true;
}

void PageManager::handleEvent(const PageEvent& event) {
  IPage* active = currentPage();
  if (active == nullptr) {
    return;
  }
  active->handleEvent(event);
  while (applyPendingAction()) {
  }
}

void PageManager::update(uint32_t nowMs) {
  IPage* active = currentPage();
  if (active == nullptr) {
    return;
  }
  active->update(nowMs);
  while (applyPendingAction()) {
  }
}

void PageManager::render() {
  IPage* active = currentPage();
  if (active == nullptr) {
    return;
  }
  active->render();
  while (applyPendingAction()) {
  }
}

IPage* PageManager::currentPage() {
  if (depth_ == 0) {
    return nullptr;
  }
  return stack_[depth_ - 1];
}

const IPage* PageManager::currentPage() const {
  if (depth_ == 0) {
    return nullptr;
  }
  return stack_[depth_ - 1];
}

size_t PageManager::depth() const {
  return depth_;
}

bool PageManager::requestSwitch(IPage::Id pageId) {
  pendingAction_ = PendingAction::kSwitch;
  pendingTarget_ = pageId;
  return true;
}

bool PageManager::requestPush(IPage::Id pageId) {
  pendingAction_ = PendingAction::kPush;
  pendingTarget_ = pageId;
  return true;
}

bool PageManager::requestPop() {
  pendingAction_ = PendingAction::kPop;
  return true;
}

bool PageManager::applyPendingAction() {
  const PendingAction action = pendingAction_;
  const IPage::Id target = pendingTarget_;
  pendingAction_ = PendingAction::kNone;

  switch (action) {
    case PendingAction::kNone:
      return false;
    case PendingAction::kSwitch: {
      const size_t index = static_cast<size_t>(target);
      if (index >= static_cast<size_t>(IPage::Id::kCount) || registry_[index] == nullptr) {
        return false;
      }
      return switchTo(*registry_[index]);
    }
    case PendingAction::kPush: {
      const size_t index = static_cast<size_t>(target);
      if (index >= static_cast<size_t>(IPage::Id::kCount) || registry_[index] == nullptr) {
        return false;
      }
      return push(*registry_[index]);
    }
    case PendingAction::kPop:
      return pop();
    default:
      return false;
  }
}

bool PageManager::switchTo(IPage& next) {
  if (depth_ == 0) {
    stack_[0] = &next;
    depth_ = 1;
    next.onEnter();
    return true;
  }

  IPage* current = currentPage();
  if (current == &next) {
    return true;
  }

  if (current != nullptr) {
    current->onExit();
  }

  stack_[depth_ - 1] = &next;
  next.onEnter();
  return true;
}

bool PageManager::push(IPage& next) {
  if (depth_ == 0) {
    stack_[0] = &next;
    depth_ = 1;
    next.onEnter();
    return true;
  }

  if (depth_ >= kMaxDepth) {
    return false;
  }

  IPage* current = currentPage();
  if (current == &next) {
    return true;
  }

  if (current != nullptr) {
    current->onExit();
  }

  stack_[depth_] = &next;
  ++depth_;
  next.onEnter();
  return true;
}

bool PageManager::pop() {
  if (depth_ <= 1) {
    return false;
  }

  IPage* current = currentPage();
  if (current != nullptr) {
    current->onExit();
  }

  stack_[depth_ - 1] = nullptr;
  --depth_;

  IPage* top = currentPage();
  if (top != nullptr) {
    top->onEnter();
  }

  return true;
}
