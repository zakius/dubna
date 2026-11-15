// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_
#define EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's cookieStoreId,
// the cookie store a tab's pages use. The default and the private store are
// Chromium's cookie stores "0" and "1". A container's is this prefix and the
// container's id (//chrome/browser/containers), as the containers sync writes
// it in Multi-Account Containers' format.
inline constexpr char kFirefoxDefaultCookieStore[] = "firefox-default";
inline constexpr char kFirefoxPrivateCookieStore[] = "firefox-private";
inline constexpr char kFirefoxContainerPrefix[] = "firefox-container-";

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_
