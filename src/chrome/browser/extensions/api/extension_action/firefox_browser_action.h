// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_BROWSER_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_BROWSER_ACTION_H_

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

// Firefox add-ons (dubna's webextensions/README.md): Firefox's browserAction
// has the badge text colour and the user settings that Chromium gives only to
// action, in Manifest V3. They run action's functions.

namespace extensions {

class BrowserActionSetBadgeTextColorFunction
    : public ActionSetBadgeTextColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeTextColor", UNKNOWN)

 protected:
  ~BrowserActionSetBadgeTextColorFunction() override = default;
};

class BrowserActionGetBadgeTextColorFunction
    : public ActionGetBadgeTextColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeTextColor", UNKNOWN)

 protected:
  ~BrowserActionGetBadgeTextColorFunction() override = default;
};

class BrowserActionGetUserSettingsFunction
    : public ActionGetUserSettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getUserSettings", UNKNOWN)

 protected:
  ~BrowserActionGetUserSettingsFunction() override = default;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_BROWSER_ACTION_H_
