// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_SHOW_MATCHES_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_SHOW_MATCHES_H_

#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// page_action.show_matches and hide_matches (extensions/common/
// firefox_page_action.h) in a tab. Each navigation of the tab's page, also
// one in the same document, such as history.pushState(), and each add-on that
// loads match the tab's URL again, as Firefox does, and the page action's
// GetIsVisible() reads the result (FirefoxActionValues::SetShowMatches()).
// show() and hide() win over it until the tab's next navigation to another
// document, which clears them (ExtensionActionDispatcher::
// ClearAllValuesForTab()). TabHelper makes one for each tab.
class FirefoxShowMatches
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FirefoxShowMatches>,
      public ExtensionRegistryObserver {
 public:
  FirefoxShowMatches(const FirefoxShowMatches&) = delete;
  FirefoxShowMatches& operator=(const FirefoxShowMatches&) = delete;
  ~FirefoxShowMatches() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* navigation) override;
  void WebContentsDestroyed() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

 private:
  friend class content::WebContentsUserData<FirefoxShowMatches>;

  explicit FirefoxShowMatches(content::WebContents* web_contents);

  // Matches the tab's URL against `extension`'s show_matches, or forgets the
  // tab with `destroyed`, and tells the page action's observers.
  void Update(const Extension& extension, bool destroyed);

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_SHOW_MATCHES_H_
