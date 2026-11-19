// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_tab_status.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

// static
bool FirefoxTabStatus::Reaches(Audience audience, const Extension* extension) {
  bool firefox_add_on = extension && IsFirefoxAddOn(*extension);
  if (audience == Audience::kFirefoxAddOns) {
    return firefox_add_on;
  }
  if (audience == Audience::kOtherExtensions) {
    return !firefox_add_on;
  }
  return true;
}

bool FirefoxTabStatus::Started(content::NavigationHandle& navigation) {
  if (!navigation.IsInPrimaryMainFrame() || navigation.IsSameDocument()) {
    return false;
  }
  waiting_ = true;
  return true;
}

bool FirefoxTabStatus::TrackedWhileLoading(content::WebContents& contents) {
  // A navigation that started or committed since the tab was tracked has
  // sent its own "loading".
  if (waiting_ || !contents.IsLoading()) {
    return false;
  }
  waiting_ = true;
  return true;
}

FirefoxTabStatus::Audience FirefoxTabStatus::Committed(bool is_main_frame) {
  if (!is_main_frame) {
    return Audience::kOtherExtensions;
  }
  waiting_ = true;
  return Audience::kAll;
}

std::optional<FirefoxTabStatus::Audience> FirefoxTabStatus::Stopped(
    bool router_waiting) {
  bool firefox_waiting = waiting_;
  waiting_ = false;
  if (router_waiting && firefox_waiting) {
    return Audience::kAll;
  }
  if (router_waiting) {
    return Audience::kOtherExtensions;
  }
  // A top frame's navigation that stopped before it committed, such as a
  // download or an HTTP 204: Firefox sends "complete" for it too.
  if (firefox_waiting) {
    return Audience::kFirefoxAddOns;
  }
  return std::nullopt;
}

}  // namespace extensions
