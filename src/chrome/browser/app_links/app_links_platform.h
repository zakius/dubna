// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_APP_LINKS_APP_LINKS_PLATFORM_H_
#define CHROME_BROWSER_APP_LINKS_APP_LINKS_PLATFORM_H_

#include <optional>
#include <string>

class GURL;

namespace content {
class WebContents;
}

// What links out of installed web apps need from the browser they run in.
// dubna has no platform code: each product defines these in its
// //chrome/browser/app_links/platform target, which the browser links.
namespace app_links {

// The id of the installed web app whose own window shows `contents`, as
// app_scope_rules.h keys its rules; nullopt for anything else, such as a tab
// or an isolated web app. Popups (window.open with window features) are told
// apart by app_links.cc.
std::optional<std::string> PlatformAppId(content::WebContents* contents);

// Whether the browser keeps `url` in the app of `contents`' window: its
// manifest scope and scope extensions. The user's rules apply over this.
bool PlatformIsUrlInScope(content::WebContents* contents, const GURL& url);

// Opens an http(s) `url` in the system default browser, whichever it is,
// for a page of `contents`. With `background`, the default browser doesn't
// take the focus, where the platform allows that.
void OpenInDefaultBrowser(content::WebContents* contents,
                          const GURL& url,
                          bool background);

// The user's scope rules changed: `contents`' window updates where it shows
// that its page is out of the app.
void OnScopeRulesChanged(content::WebContents* contents);

}  // namespace app_links

#endif  // CHROME_BROWSER_APP_LINKS_APP_LINKS_PLATFORM_H_
