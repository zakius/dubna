// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_page_action_api.h"

#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/firefox_page_action_buttons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "extensions/browser/extension_action.h"

namespace extensions {

ExtensionFunction::ResponseAction
PageActionIsShownFunction::RunExtensionAction() {
  return RespondNow(WithArguments(
      extension_action_->GetIsVisibleIgnoringDeclarative(tab_id_)));
}

ExtensionFunction::ResponseAction PageActionOpenPopupFunction::Run() {
  if (!user_gesture()) {
    return RespondNow(
        Error("pageAction.openPopup may only be called from a user input "
              "handler"));
  }
  BrowserWindowInterface* top_window = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        WindowController* window =
            BrowserExtensionWindowController::From(browser);
        if (!window || !windows_util::CanOperateOnWindow(
                           this, window, WindowController::kNoWindowFilter)) {
          return true;
        }
        top_window = browser;
        return false;
      });
  FirefoxPageActionButtons* buttons =
      top_window ? FirefoxPageActionButtons::From(*top_window) : nullptr;
  if (!buttons || !buttons->RunPageAction(*extension())) {
    return RespondNow(Error("Failed to open popup."));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
