// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_AMO_USER_AGENT_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_AMO_USER_AGENT_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's user agent on
// this platform, with Chromium's major version as Firefox's, for example
// "Mozilla/5.0 (X11; Linux x86_64; rv:157.0) Gecko/20100101 Firefox/157.0".
std::string FirefoxUserAgent();

// addons.mozilla.org (AMO) shows its "Add to Firefox" button only to a
// browser whose user agent names Firefox, in the User-Agent header of the
// page and in navigator.userAgent. Without navigator.mozAddonManager the
// button is a link to the add-on's .xpi, whose download opens the install
// prompt (firefox_xpi_installer.h). So a tab's pages on AMO get
// FirefoxUserAgent(), with their subframes and subresources, as in Firefox,
// and its pages on other hosts keep Chromium's. A page that a redirect or a
// prerender commits with the other one loads again.
class FirefoxAmoUserAgent
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FirefoxAmoUserAgent> {
 public:
  FirefoxAmoUserAgent(const FirefoxAmoUserAgent&) = delete;
  FirefoxAmoUserAgent& operator=(const FirefoxAmoUserAgent&) = delete;
  ~FirefoxAmoUserAgent() override;

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* navigation) override;
  void DidFinishNavigation(content::NavigationHandle* navigation) override;

 private:
  friend class content::WebContentsUserData<FirefoxAmoUserAgent>;

  explicit FirefoxAmoUserAgent(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_AMO_USER_AGENT_H_
