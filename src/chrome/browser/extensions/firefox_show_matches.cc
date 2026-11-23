// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_show_matches.h"

#include <optional>

#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/firefox_action_values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/firefox_page_action.h"

namespace extensions {

namespace {

// The add-on's page action: the one next to its other action, or Chromium's.
ExtensionAction* PageActionOf(content::BrowserContext* context,
                              const Extension& extension) {
  ExtensionActionManager* manager = ExtensionActionManager::Get(context);
  if (ExtensionAction* page_action = manager->GetFirefoxPageAction(extension)) {
    return page_action;
  }
  ExtensionAction* action = manager->GetExtensionAction(extension);
  if (action && action->action_type() == ActionInfo::Type::kPage) {
    return action;
  }
  return nullptr;
}

}  // namespace

FirefoxShowMatches::FirefoxShowMatches(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FirefoxShowMatches>(*web_contents) {
  registry_observation_.Observe(
      ExtensionRegistry::Get(web_contents->GetBrowserContext()));
}

FirefoxShowMatches::~FirefoxShowMatches() = default;

void FirefoxShowMatches::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation->IsInPrimaryMainFrame() || !navigation->HasCommitted()) {
    return;
  }
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext())
          ->enabled_extensions();
  for (const auto& extension : extensions) {
    Update(*extension, /*destroyed=*/false);
  }
}

void FirefoxShowMatches::WebContentsDestroyed() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext())
          ->enabled_extensions();
  for (const auto& extension : extensions) {
    Update(*extension, /*destroyed=*/true);
  }
}

void FirefoxShowMatches::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  Update(*extension, /*destroyed=*/false);
}

void FirefoxShowMatches::Update(const Extension& extension, bool destroyed) {
  const std::optional<bool> matches =
      MatchFirefoxShowMatches(extension, web_contents()->GetLastCommittedURL());
  if (!matches) {
    return;
  }
  content::BrowserContext* context = web_contents()->GetBrowserContext();
  ExtensionAction* action = PageActionOf(context, extension);
  if (!action) {
    return;
  }
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  if (destroyed) {
    FirefoxActionValues::SetShowMatches(*action, tab_id, std::nullopt);
    return;
  }
  FirefoxActionValues::SetShowMatches(*action, tab_id, matches);
  ExtensionActionDispatcher::Get(context)->NotifyChange(action, web_contents(),
                                                        context);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FirefoxShowMatches);

}  // namespace extensions
