// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_clicks.h"

#include <string>

#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

constexpr char kSidebarActionCommand[] = "_execute_sidebar_action";

// The toolbar action of `extension` in the window of `web_contents`, run as
// its keyboard shortcut runs it.
void RunToolbarAction(const Extension& extension,
                      content::WebContents& web_contents) {
  BrowserWindowInterface* browser =
      browser_window_util::GetBrowserForTabContents(web_contents);
  ExtensionsContainer* container =
      browser ? ExtensionsContainer::From(*browser) : nullptr;
  ToolbarActionViewModel* action =
      container ? container->GetActionForId(extension.id()) : nullptr;
  if (action) {
    action->ExecuteUserAction(
        ToolbarActionViewModel::InvocationSource::kCommand);
  }
}

}  // namespace

bool RunFirefoxMenuCommand(const Extension* extension,
                           const MenuItem& item,
                           content::WebContents* web_contents) {
  if (!extension || !web_contents || !IsFirefoxAddOn(*extension)) {
    return false;
  }
  const std::string& command = item.firefox_command();
  const char* action_command = extension->manifest_version() >= 3
                                   ? manifest_values::kActionCommandEvent
                                   : manifest_values::kBrowserActionCommandEvent;
  if (command == action_command) {
    RunToolbarAction(*extension, *web_contents);
    return true;
  }
  return command == manifest_values::kPageActionCommandEvent ||
         command == kSidebarActionCommand;
}

}  // namespace extensions
