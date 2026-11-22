// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_tab_menus.h"

#include <algorithm>
#include <functional>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool MatchesWithDocumentUrl(
    const base::RepeatingCallback<bool(const MenuItem*)>& filter,
    const GURL& url,
    const MenuItem* item) {
  if (!filter.Run(item)) {
    return false;
  }
  if (!item->firefox_add_on()) {
    return true;
  }
  // These menus have no link or medium for targetUrlPatterns to match.
  if (!item->firefox_target_url_patterns().empty()) {
    return false;
  }
  const URLPatternSet& patterns = item->document_url_patterns();
  return patterns.is_empty() || patterns.MatchesURL(url);
}

// When a Firefox add-on was installed, and for other extensions no time,
// which comes first.
base::Time FirefoxInstallTime(content::BrowserContext* context,
                              const MenuItem::ExtensionKey& key) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          key.extension_id);
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return base::Time();
  }
  return GetFirstInstallTime(ExtensionPrefs::Get(context), key.extension_id);
}

}  // namespace

base::RepeatingCallback<bool(const MenuItem*)> WithFirefoxDocumentUrl(
    base::RepeatingCallback<bool(const MenuItem*)> filter,
    content::WebContents* tab) {
  GURL url = tab ? tab->GetLastCommittedURL() : GURL();
  return base::BindRepeating(&MatchesWithDocumentUrl, std::move(filter), url);
}

std::vector<MenuItem::ExtensionKey> FirefoxTabMenuExtensions(
    content::BrowserContext* context) {
  std::set<MenuItem::ExtensionKey> ids =
      MenuManager::Get(context)->ExtensionIds();
  std::vector<MenuItem::ExtensionKey> ordered(ids.begin(), ids.end());
  std::ranges::stable_sort(ordered, std::ranges::less(),
                           [context](const MenuItem::ExtensionKey& key) {
                             return FirefoxInstallTime(context, key);
                           });
  return ordered;
}

}  // namespace extensions
