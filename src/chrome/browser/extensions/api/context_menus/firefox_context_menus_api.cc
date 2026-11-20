// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/context_menus/firefox_context_menus_api.h"

#include "chrome/browser/extensions/firefox_menu_events.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

ExtensionFunction::ResponseAction ContextMenusRefreshFunction::Run() {
  // Firefox's function: another extension's call changes nothing, as its
  // items' menus are Chromium's.
  if (extension() && IsFirefoxAddOn(*extension())) {
    RefreshFirefoxMenu(browser_context(), extension_id());
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
