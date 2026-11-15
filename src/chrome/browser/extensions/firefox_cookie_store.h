// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_COOKIE_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_COOKIE_STORE_H_

#include <optional>
#include <string>
#include <string_view>

#include "extensions/common/firefox_cookie_store.h"

namespace content {
class WebContents;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the cookie store of
// `contents` as Firefox's cookieStoreId (extensions/common/
// firefox_cookie_store.h): the private one in an incognito profile, a
// container's, or the default one.
std::string FirefoxCookieStoreId(content::WebContents* contents);

// The container id in a container's cookieStoreId, or nullopt for another
// cookie store.
std::optional<std::string> ContainerOfFirefoxCookieStore(
    std::string_view cookie_store_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_COOKIE_STORE_H_
