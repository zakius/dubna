// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_closing_tab.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace extensions {

FirefoxClosingTab::FirefoxClosingTab() = default;

FirefoxClosingTab::~FirefoxClosingTab() = default;

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
  closing_ = reason == tabs::TabInterface::DetachReason::kDelete;
  if (closing_) {
    // The tab is detached in this task, and its pinned update with it; a
    // tab still there afterwards stayed open.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FirefoxClosingTab::CloseEnded,
                                  weak_factory_.GetWeakPtr()));
  }
}

void FirefoxClosingTab::CloseEnded() {
  closing_ = false;
}

}  // namespace extensions
