// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_window_activation.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "ui/base/base_window.h"

namespace extensions {

namespace {

// How often to look whether the emptied window is gone.
constexpr base::TimeDelta kInterval = base::Milliseconds(10);
// How long after that the window system's activation may still come.
constexpr base::TimeDelta kAfterGone = base::Milliseconds(100);
// The longest the response waits.
constexpr base::TimeDelta kTimeLimit = base::Seconds(1);

// Watches the activation of other browsers until the wait is over, then puts
// the new window first again if one took the lead, responds, and deletes
// itself.
class Waiter : public BrowserCollectionObserver {
 public:
  Waiter(base::WeakPtr<BrowserWindowInterface> window,
         base::WeakPtr<BrowserWindowInterface> emptied_window,
         base::OnceClosure respond)
      : window_(std::move(window)),
        emptied_window_(std::move(emptied_window)),
        respond_(std::move(respond)),
        deadline_(base::TimeTicks::Now() + kTimeLimit) {
    observation_.Observe(GlobalBrowserCollection::GetInstance());
    timer_.Start(FROM_HERE, kInterval,
                 base::BindRepeating(&Waiter::Check, base::Unretained(this)));
  }
  Waiter(const Waiter&) = delete;
  Waiter& operator=(const Waiter&) = delete;
  ~Waiter() override = default;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override {
    // A window the user or an add-on activates deactivates the new window's
    // widget first. Only the window system's stray activation leaves it
    // active.
    if (window_ && browser != window_.get() &&
        window_->GetWindow()->IsActive()) {
      other_activated_ = true;
    }
  }

 private:
  void Check() {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (!emptied_window_ && gone_.is_null()) {
      gone_ = now;
    }
    const bool settled = !gone_.is_null() && now - gone_ >= kAfterGone;
    if (!settled && now < deadline_) {
      return;
    }
    timer_.Stop();
    observation_.Reset();
    if (other_activated_ && window_ && window_->GetWindow()->IsActive() &&
        GetLastActiveBrowserWindowInterfaceWithAnyProfile() != window_.get()) {
      // Activated again, the window leads the order, as after any
      // activation. The window stayed active, so it has to leave that state
      // first.
      BrowserActiveStateManager* state =
          BrowserActiveStateManager::From(window_.get());
      state->DidBecomeInactive();
      state->DidBecomeActive();
    }
    std::move(respond_).Run();
    delete this;
  }

  base::WeakPtr<BrowserWindowInterface> window_;
  base::WeakPtr<BrowserWindowInterface> emptied_window_;
  base::OnceClosure respond_;
  const base::TimeTicks deadline_;
  // When the emptied window was found gone.
  base::TimeTicks gone_;
  bool other_activated_ = false;
  base::RepeatingTimer timer_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

}  // namespace

FirefoxWindowActivation::FirefoxWindowActivation(
    const Extension* extension,
    WindowController* source_window) {
  if (!extension || !IsFirefoxAddOn(*extension) || !source_window) {
    return;
  }
  BrowserWindowInterface* browser = source_window->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  moves_tab_ = true;
  source_window_ = browser->GetWeakPtr();
}

FirefoxWindowActivation::~FirefoxWindowActivation() = default;

bool FirefoxWindowActivation::Waits(BrowserWindowInterface* new_window) {
  // Not focused, or shown inactive: the new window isn't the current one in
  // Firefox either.
  if (!moves_tab_ || !new_window->IsActive()) {
    return false;
  }
  // The tab's window is gone, or closes now that it has no tabs.
  if (source_window_ && !source_window_->IsDeleteScheduled()) {
    TabListInterface* tabs = TabListInterface::From(source_window_.get());
    if (!tabs || tabs->GetTabCount() > 0) {
      return false;
    }
  }
  new_window_ = new_window->GetWeakPtr();
  return true;
}

void FirefoxWindowActivation::RespondLater(base::OnceClosure respond) {
  // Deletes itself.
  new Waiter(new_window_, source_window_, std::move(respond));
}

}  // namespace extensions
