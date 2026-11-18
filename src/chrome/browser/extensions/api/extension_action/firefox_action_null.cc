// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): null in an action's
// setter clears the value at that level (a tab's, a window's or the global
// one), as in Firefox, and setIcon() with an empty path shows the generic
// icon. The schemas make these properties optional, so a null reaches the
// browser as a missing property, and set_icon.js sends a setIcon() without
// an image as it is.

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/firefox_action_values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/firefox_page_action.h"

namespace extensions {

namespace {

using Value = FirefoxActionValues::Value;

bool IsNull(const base::DictValue& details, std::string_view key) {
  const base::Value* value = details.Find(key);
  return !value || value->is_none();
}

// The value that `function`'s details clear, if they do. `empty_icon` is set
// for setIcon() with an empty path.
std::optional<Value> ClearedValue(std::string_view function,
                                  const base::DictValue& details,
                                  bool& empty_icon) {
  function = function.substr(function.find('.') + 1);
  if (function == "setTitle" && IsNull(details, "title")) {
    return Value::kTitle;
  }
  if (function == "setPopup" && IsNull(details, "popup")) {
    return Value::kPopup;
  }
  if (function == "setBadgeBackgroundColor" && IsNull(details, "color")) {
    return Value::kBadgeBackgroundColor;
  }
  if (function == "setBadgeTextColor" && IsNull(details, "color")) {
    return Value::kBadgeTextColor;
  }
  if (function == "setIcon" && IsNull(details, "imageData") &&
      IsNull(details, "iconIndex")) {
    const std::string* path = details.FindString("path");
    empty_icon = path && path->empty();
    return Value::kIcon;
  }
  return std::nullopt;
}

}  // namespace

ExtensionFunction::ResponseAction ExtensionActionFunction::RunFirefoxAction() {
  bool empty_icon = false;
  std::optional<Value> value =
      details_ ? ClearedValue(name(), *details_, empty_icon) : std::nullopt;
  if (!value) {
    return RunExtensionAction();
  }
  if (empty_icon) {
    extension_action_->SetIcon(tab_id_, ExtensionAction::FallbackIcon());
  } else {
    // The manifest's values of this action: the add-on's one action, or the
    // page action next to it.
    const ActionInfo* info =
        extension_action_.get() ==
                ExtensionActionManager::Get(browser_context())
                    ->GetFirefoxPageAction(*extension())
            ? GetFirefoxPageActionInfo(*extension())
            : ActionInfo::GetExtensionActionInfo(extension());
    ExtensionAction defaults(*extension(), *info);
    FirefoxActionValues::Clear(*extension_action_, defaults, *value, tab_id_);
  }
  NotifyChange();
  return RespondNow(NoArguments());
}

}  // namespace extensions
