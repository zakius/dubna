// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_action_window.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/firefox_action_window.h"

namespace extensions {

namespace {

int WindowOfTab(int tab_id) {
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    for (int i = 0; i < window->GetTabCount(); ++i) {
      content::WebContents* contents = window->GetWebContentsAt(i);
      if (contents && ExtensionTabUtil::GetTabId(contents) == tab_id) {
        return window->GetWindowId();
      }
    }
  }
  return ExtensionAction::kDefaultTabId;
}

}  // namespace

bool TakeFirefoxWindowId(ExtensionFunction& function,
                         const base::DictValue* details,
                         int& key,
                         std::string& error) {
  std::optional<int> window_id =
      details ? details->FindInt("windowId") : std::nullopt;
  if (!window_id) {
    return false;
  }
  // Firefox's messages.
  if (details->FindInt("tabId")) {
    error = "Only one of tabId and windowId can be specified.";
    return true;
  }
  if (!WindowControllerList::GetInstance()->FindWindowForFunctionByIdWithFilter(
          &function, *window_id, WindowController::kNoWindowFilter)) {
    error = "Invalid window ID: " + base::NumberToString(*window_id);
    return true;
  }
  SetFirefoxWindowOfTab(&WindowOfTab);
  key = *window_id;
  return true;
}

}  // namespace extensions
