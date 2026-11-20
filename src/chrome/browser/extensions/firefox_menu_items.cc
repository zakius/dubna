// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_items.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace extensions {

namespace {

constexpr char kInvalidURLPatternError[] = "Invalid url pattern '*'";

// The schemes of Firefox's <all_urls> (MatchPattern.cpp's PermittedSchemes).
constexpr auto kAllUrlsSchemes = base::MakeFixedFlatSet<std::string_view>(
    {"data", "file", "ftp", "http", "https", "ws", "wss"});

// A pattern of a scheme with a host ("https://*/*", or any scheme with "*")
// matches as Chromium's URLPattern does, which is Firefox's MatchPattern for
// these schemes. A pattern of any other scheme matches the URL after the
// scheme's colon, without its fragment, with * as the only wildcard, as
// Firefox's does for schemes without a host. Firefox's own list of schemes
// with a host has moz-extension where this browser has chrome-extension, a
// standard scheme of GURL.
bool HasHost(std::string_view scheme) {
  return scheme == "*" || url::IsStandard(scheme);
}

bool IsValidPattern(std::string_view pattern) {
  if (pattern == URLPattern::kAllUrlsPattern) {
    return true;
  }
  size_t colon = pattern.find(':');
  if (colon == std::string_view::npos || colon == 0) {
    return false;
  }
  if (HasHost(pattern.substr(0, colon))) {
    return URLPattern(URLPattern::SCHEME_ALL).Parse(pattern) ==
           URLPattern::ParseResult::kSuccess;
  }
  return colon + 1 < pattern.size();
}

bool PatternMatches(std::string_view pattern, const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  if (pattern == URLPattern::kAllUrlsPattern) {
    return kAllUrlsSchemes.contains(url.scheme());
  }
  size_t colon = pattern.find(':');
  std::string_view scheme = pattern.substr(0, colon);
  if (HasHost(scheme)) {
    URLPattern host_pattern(URLPattern::SCHEME_ALL);
    return host_pattern.Parse(pattern) == URLPattern::ParseResult::kSuccess &&
           host_pattern.MatchesURL(url);
  }
  if (url.scheme() != scheme) {
    return false;
  }
  std::string spec = url.GetWithoutRef().spec();
  std::string_view rest = std::string_view(spec).substr(scheme.size() + 1);
  // base::MatchPattern also takes ? as a wildcard and \ as an escape.
  std::string glob(pattern.substr(colon + 1));
  base::ReplaceSubstringsAfterOffset(&glob, 0, "\\", "\\\\");
  base::ReplaceSubstringsAfterOffset(&glob, 0, "?", "\\?");
  return base::MatchPattern(rest, glob);
}

bool AnyPatternMatches(const std::vector<std::string>& patterns,
                       const GURL& url) {
  for (const std::string& pattern : patterns) {
    if (PatternMatches(pattern, url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

MenuItem::ContextList FirefoxDefaultMenuContexts(
    const MenuManager& menu_manager,
    const MenuItem::Id* parent_id) {
  const MenuItem* parent =
      parent_id ? menu_manager.GetItemById(*parent_id) : nullptr;
  if (parent) {
    return parent->contexts();
  }
  return MenuItem::ContextList(MenuItem::ALL);
}

bool SetFirefoxTargetUrlPatterns(const std::vector<std::string>& patterns,
                                 MenuItem& item,
                                 std::string* error) {
  for (const std::string& pattern : patterns) {
    if (!IsValidPattern(pattern)) {
      *error = ErrorUtils::FormatErrorMessage(kInvalidURLPatternError, pattern);
      return false;
    }
  }
  item.set_firefox_target_url_patterns(patterns);
  return true;
}

bool FirefoxMenuItemMatchesContext(const content::ContextMenuParams& params,
                                   const MenuItem& item) {
  const std::vector<std::string>& patterns =
      item.firefox_target_url_patterns();
  if (!patterns.empty()) {
    bool on_media =
        params.media_type == blink::mojom::ContextMenuDataMediaType::kImage ||
        params.media_type == blink::mojom::ContextMenuDataMediaType::kVideo ||
        params.media_type == blink::mojom::ContextMenuDataMediaType::kAudio;
    bool src_matches = on_media && AnyPatternMatches(patterns, params.src_url);
    // The link's URL as the page has it: Chromium filters a javascript: link
    // to about:blank#blocked in link_url.
    bool link_matches = AnyPatternMatches(patterns, params.unfiltered_link_url);
    if (!src_matches && !link_matches) {
      return false;
    }
  }
  // The contexts alone: the patterns matched above. A link whose URL isn't
  // valid gets a link_url that isn't empty, so that it has the link context
  // and not the page's.
  if (params.link_url.is_empty() && !params.invalid_link_url.empty()) {
    content::ContextMenuParams link_params(params);
    link_params.link_url = GURL(params.invalid_link_url);
    return context_menu_helpers::ExtensionContextAndPatternMatch(
        link_params, item.contexts(), URLPatternSet());
  }
  return context_menu_helpers::ExtensionContextAndPatternMatch(
      params, item.contexts(), URLPatternSet());
}

}  // namespace extensions
