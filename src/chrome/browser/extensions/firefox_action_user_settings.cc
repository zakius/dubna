// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_action_user_settings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/event_args.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"

namespace extensions {

void DispatchFirefoxBrowserActionUserSettingsChanged(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    bool is_pinned) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  const ActionInfo* action = ActionInfo::GetExtensionActionInfo(extension);
  if (!action || action->type != ActionInfo::Type::kBrowser) {
    return;
  }
  EventRouter* router = EventRouter::Get(context);
  if (!router) {
    return;
  }

  base::DictValue change;
  change.Set("isOnToolbar", is_pinned);
  base::ListValue args;
  args.Append(std::move(change));
  router->DispatchEventToExtension(
      extension_id,
      std::make_unique<Event>(events::ACTION_ON_USER_SETTINGS_CHANGED,
                              "browserAction.onUserSettingsChanged",
                              base::MakeRefCounted<EventArgs>(std::move(args)),
                              context));
}

bool FirefoxPinnedAtInstall(const Extension& extension,
                            bool pinned_by_default) {
  if (!IsFirefoxAddOn(extension)) {
    return pinned_by_default;
  }
  const ActionInfo* action = ActionInfo::GetExtensionActionInfo(&extension);
  if (!action || action->type == ActionInfo::Type::kPage) {
    return pinned_by_default;
  }
  const std::string* area = extension.manifest()->FindStringPath(
      std::string(ActionInfo::GetManifestKeyForActionType(action->type)) +
      ".default_area");
  if (!area) {
    return pinned_by_default;
  }
  if (*area == "menupanel") {
    return false;
  }
  return *area == "navbar" || *area == "tabstrip" ||
         *area == "personaltoolbar" || pinned_by_default;
}

}  // namespace extensions
