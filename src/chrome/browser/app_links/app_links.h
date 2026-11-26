// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_APP_LINKS_APP_LINKS_H_
#define CHROME_BROWSER_APP_LINKS_APP_LINKS_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/app_links/app_link_host.mojom.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace content {
class NavigationThrottleRegistry;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Links out of installed web apps: a link in an app's own window that leaves
// the app's scope opens in the system default browser, as a link from any
// other app does. The page's own ways to change its address ask the browser
// first (third_party/blink/renderer/core/html/app_links.h); a throttle and
// an observer catch what they miss; new windows are checked where the
// browser adds them. The scope is the browser's with the user's rules
// (app_scope_rules.h). What it needs from the browser is in
// app_links_platform.h.
namespace app_links {

// Whether `contents` is shown in an installed web app's own window, not a
// popup of it. Such a window has no prerendered pages: activating one skips
// the navigation throttle.
bool IsInAppWindow(content::WebContents* contents);

// For the browser's handling of a new window that `source` opens on `url`
// (a new tab or window, or a popup) with `disposition`; `new_contents` is the
// window's WebContents where it exists already. From an app's own window, a
// new tab or window out of the app's scope goes to the default browser (on
// Android, one in the scope too, unless the user's rules keep it), and with a
// strict scope, so does a popup. Returns true when it took the
// navigation, which the caller then drops. Form posts stay: only Chromium can
// submit them. A popup that stays is remembered as one, so its links stay
// with it.
bool MaybeOpenNewContentsOutsideApp(content::WebContents* source,
                                    const GURL& url,
                                    WindowOpenDisposition disposition,
                                    bool post,
                                    content::WebContents* new_contents);

// A link clicked in an app's own window that leaves the app's scope, which
// would load in the window, goes to the default browser instead; so does a
// same-document navigation from a click that leaves it. So does any URL the
// page reaches that one of the user's exclusions matches, or with a strict
// scope, any URL out of the scope: also by script or redirect. The window
// undoes a same-document navigation.
void MaybeAddThrottle(content::NavigationThrottleRegistry& registry);

// Binds the host that a main frame in an app's window asks, before its page
// changes its address (navigates, pushState, replaceState, hash), whether
// the URL leaves the app; if it does, the address doesn't change, and the
// host opens the URL in the default browser, once per user input.
void BindAppLinkHost(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::AppLinkHost> receiver);

}  // namespace app_links

#endif  // CHROME_BROWSER_APP_LINKS_APP_LINKS_H_
