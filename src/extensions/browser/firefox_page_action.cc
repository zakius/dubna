// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include <memory>

#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_page_action.h"

namespace extensions {

// Firefox add-ons (extensions/common/firefox_page_action.h): made as
// GetExtensionAction() makes the add-on's one action.
ExtensionAction* ExtensionActionManager::GetFirefoxPageAction(
    const Extension& extension) const {
  auto iter = firefox_page_actions_.find(extension.id());
  if (iter != firefox_page_actions_.end()) {
    return iter->second.get();
  }

  const ActionInfo* action_info = GetFirefoxPageActionInfo(extension);
  if (!action_info || !ExtensionRegistry::Get(browser_context_)
                           ->enabled_extensions()
                           .Contains(extension.id())) {
    return nullptr;
  }

  auto action = std::make_unique<ExtensionAction>(extension, *action_info);
  if (action->default_icon()) {
    action->SetDefaultIconImage(std::make_unique<IconImage>(
        browser_context_, &extension, *action->default_icon(),
        ExtensionAction::ActionIconSize(),
        ExtensionAction::FallbackIcon().AsImageSkia(), nullptr));
  }

  ExtensionAction* raw_action = action.get();
  firefox_page_actions_[extension.id()] = std::move(action);
  return raw_action;
}

}  // namespace extensions
