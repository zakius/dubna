// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_action_click.h"

#include <utility>

#include "build/build_config.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace extensions {

namespace {

// The click that the action runs for, while a ScopedFirefoxActionClick lives.
int g_button = 0;
int g_event_flags = 0;

bool IsMiddleButton(const ui::Event& event) {
  return event.IsMouseEvent() && (event.AsMouseEvent()->changed_button_flags() &
                                  ui::EF_MIDDLE_MOUSE_BUTTON);
}

}  // namespace

ScopedFirefoxActionClick::ScopedFirefoxActionClick(const ui::Event& event)
    : button_(&g_button, IsMiddleButton(event) ? 1 : 0),
      event_flags_(&g_event_flags, event.flags()) {}

ScopedFirefoxActionClick::~ScopedFirefoxActionClick() = default;

bool IsFirefoxMiddleClick(const ui::Event& event,
                          content::WebContents* web_contents,
                          const ExtensionId& extension_id) {
  if (event.type() != ui::EventType::kMouseReleased ||
      !IsMiddleButton(event) || !web_contents) {
    return false;
  }
  const Extension* extension =
      ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->enabled_extensions()
          .GetByID(extension_id);
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return false;
  }
  // A disabled action does nothing on a middle click, as in Firefox.
  const ExtensionAction* action =
      ExtensionActionManager::Get(web_contents->GetBrowserContext())
          ->GetExtensionAction(*extension);
  return action && action->GetIsVisible(
                       sessions::SessionTabHelper::IdForTab(web_contents).id());
}

bool IsFirefoxMiddleClickRunning(const Extension& extension) {
  return g_button == 1 && IsFirefoxAddOn(extension);
}

void AddFirefoxOnClickData(const Extension* extension, base::ListValue& args) {
#if !BUILDFLAG(IS_ANDROID)
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  base::DictValue info;
  info.Set("button", g_button);
  info.Set("modifiers", FirefoxClickModifiers(g_event_flags));
  args.Append(std::move(info));
#endif
}

base::ListValue FirefoxClickModifiers(int event_flags) {
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
  return modifiers;
}

}  // namespace extensions
