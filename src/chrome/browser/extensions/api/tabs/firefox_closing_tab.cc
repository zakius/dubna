// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_closing_tab.h"

#include "base/functional/bind.h"

namespace extensions {

void FirefoxClosingTab::Observe(tabs::TabInterface& tab) {
  will_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &FirefoxClosingTab::WillDetach, base::Unretained(this)));
}

FirefoxTabStatus::Audience FirefoxClosingTab::PinnedAudience() const {
  if (closing_) {
    return FirefoxTabStatus::Audience::kOtherExtensions;
  }
  return FirefoxTabStatus::Audience::kAll;
}

void FirefoxClosingTab::WillDetach(tabs::TabInterface* tab,
                                   tabs::TabInterface::DetachReason reason) {
  // A tab detached to be deleted is never inserted again.
  closing_ = reason == tabs::TabInterface::DetachReason::kDelete;
}

}  // namespace extensions
