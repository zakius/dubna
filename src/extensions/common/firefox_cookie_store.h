// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_
#define EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_

#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's cookieStoreId,
// the cookie store a tab's pages use. The default and the private store are
// Chromium's cookie stores "0" and "1". A container's is this prefix and the
// container's id (//chrome/browser/containers), as the containers sync writes
// it in Multi-Account Containers' format.
inline constexpr char kFirefoxDefaultCookieStore[] = "firefox-default";
inline constexpr char kFirefoxPrivateCookieStore[] = "firefox-private";
inline constexpr char kFirefoxContainerPrefix[] = "firefox-container-";

// The container id in a container's cookieStoreId, or nullopt for another
// cookie store.
inline std::optional<std::string> ContainerOfFirefoxCookieStore(
    std::string_view cookie_store_id) {
  if (!base::StartsWith(cookie_store_id, kFirefoxContainerPrefix) ||
      cookie_store_id.size() == std::size(kFirefoxContainerPrefix) - 1) {
    return std::nullopt;
  }
  return std::string(
      cookie_store_id.substr(std::size(kFirefoxContainerPrefix) - 1));
}

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_COOKIE_STORE_H_
