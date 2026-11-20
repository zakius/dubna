// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_discarded_tab.h"

#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/common/constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

std::optional<std::string> FirefoxDiscardedTabError(
    const api::tabs::Create::Params::CreateProperties& properties,
    const GURL& url) {
  if (!properties.discarded.value_or(false)) {
    // Firefox tests the title as JavaScript does, so an empty one passes.
    if (properties.title && !properties.title->empty()) {
      return "Title may only be set for discarded tabs.";
    }
    return std::nullopt;
  }
  // `selected` is Chromium's older name for `active`.
  std::optional<bool> active =
      properties.active ? properties.active : properties.selected;
  if (active.value_or(false)) {
    return "Active tabs cannot be created and discarded.";
  }
  if (properties.pinned.value_or(false)) {
    return "Pinned tabs cannot be created and discarded.";
  }
  // Firefox's about: pages are the browser's own pages here, chrome:// and
  // the browser's own scheme, the new tab page among them. A discarded tab is
  // inserted without the navigator's checks of a URL, such as the one that
  // keeps settings pages out of private windows, so only the pages a web page
  // or the add-on itself could open are taken.
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile() &&
      !url.SchemeIs(url::kDataScheme) && !url.SchemeIs(kExtensionScheme)) {
    return "Cannot create a discarded new tab or \"about\" urls.";
  }
  return std::nullopt;
}

std::unique_ptr<content::WebContents> CreateFirefoxDiscardedTab(
    content::BrowserContext& browser_context,
    const GURL& url,
    const std::optional<std::string>& title) {
  // As a session restore makes a tab that waits for the user
  // (browser_tabrestore.cc): no renderer, hidden, and a restored entry, which
  // the contents load when they are shown.
  content::WebContents::CreateParams create_params(&browser_context);
  create_params.initially_hidden = true;
  create_params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(create_params);

  std::unique_ptr<content::NavigationEntry> entry =
      content::NavigationController::CreateNavigationEntry(
          url, content::Referrer(), /*initiator_origin=*/std::nullopt,
          /*initiator_base_url=*/std::nullopt, ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false, /*extra_headers=*/std::string(),
          &browser_context, /*blob_url_loader_factory=*/nullptr);
  if (title) {
    entry->SetTitle(base::UTF8ToUTF16(*title));
  }
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(std::move(entry));
  contents->GetController().Restore(/*selected_navigation=*/0,
                                    content::RestoreType::kRestored, &entries);
  return contents;
}

bool IsFirefoxDiscardedTab(content::WebContents& contents) {
  // A tab whose subframe crashed while hidden reloads when it's shown too,
  // but its page is there, so the tab isn't unloaded.
  return contents.GetController().NeedsReload() &&
         ExtensionTabUtil::GetLoadingStatus(&contents) ==
             api::tabs::TabStatus::kUnloaded;
}

}  // namespace extensions
