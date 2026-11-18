// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_clicks.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_constants.h"
#include "ui/events/event_constants.h"

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

void AddFirefoxClickData(const Extension* extension,
                         const content::ContextMenuParams& params,
                         int event_flags,
                         base::DictValue& properties) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  // In the order of Firefox's clickModifiersFromEvent().
  base::ListValue modifiers;
  if (event_flags & ui::EF_SHIFT_DOWN) {
    modifiers.Append("Shift");
  }
  if (event_flags & ui::EF_ALT_DOWN) {
    modifiers.Append("Alt");
  }
  if (event_flags & ui::EF_COMMAND_DOWN) {
    modifiers.Append("Command");
  }
  if (event_flags & ui::EF_CONTROL_DOWN) {
    modifiers.Append("Ctrl");
#if BUILDFLAG(IS_MAC)
    modifiers.Append("MacCtrl");
#endif
  }
  properties.Set("modifiers", std::move(modifiers));

  int button = 0;
  if (event_flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
    button = 1;
  } else if (event_flags & ui::EF_RIGHT_MOUSE_BUTTON) {
    button = 2;
  }
  properties.Set("button", button);

  if (!params.unfiltered_link_url.is_empty()) {
    properties.Set("linkText", params.link_text);
  }
}

bool ContextMenuMatcher::MiddleClickRuns(int command_id) const {
  const MenuItem* item = GetExtensionMenuItem(command_id);
  return item && item->firefox_add_on();
}

}  // namespace extensions
