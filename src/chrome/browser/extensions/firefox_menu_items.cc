// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_items.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/firefox_menu_access_keys.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/url_pattern.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "ui/menus/simple_menu_model.h"
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

// Whether `contexts` match the menu of `params`, as Chromium's
// context_menu_helpers::ExtensionContextAndPatternMatch() matches them
// without patterns, but with Firefox's link and selection.
bool ContextsMatch(const content::ContextMenuParams& params,
                   const MenuItem::ContextList& contexts) {
  bool on_link =
      !params.link_url.is_empty() || !params.invalid_link_url.empty();
  bool has_selection = !params.selection_text.empty() ||
                       !params.firefox_selection_text.empty();
  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (params.is_subframe && contexts.Contains(MenuItem::FRAME)) ||
      (on_link && contexts.Contains(MenuItem::LINK))) {
    return true;
  }
  switch (params.media_type) {
    case blink::mojom::ContextMenuDataMediaType::kImage:
      return contexts.Contains(MenuItem::IMAGE);
    case blink::mojom::ContextMenuDataMediaType::kVideo:
      return contexts.Contains(MenuItem::VIDEO);
    case blink::mojom::ContextMenuDataMediaType::kAudio:
      return contexts.Contains(MenuItem::AUDIO);
    case blink::mojom::ContextMenuDataMediaType::kNone:
      // As in Chromium, the page of a frame is a page too.
      return !on_link && !has_selection && !params.is_editable &&
             contexts.Contains(MenuItem::PAGE);
    default:
      return false;
  }
}

size_t CodePointCount(std::u16string_view text) {
  size_t count = 0;
  for (char16_t c : text) {
    if (!CBU16_IS_TRAIL(c)) {
      ++count;
    }
  }
  return count;
}

std::u16string_view FirstCodePoints(std::u16string_view text, size_t count) {
  size_t end = 0;
  for (size_t taken = 0; taken < count && end < text.size(); ++taken) {
    end += CBU16_IS_LEAD(text[end]) && end + 1 < text.size() ? 2 : 1;
  }
  return text.substr(0, end);
}

// Firefox's longest label with a selection in it (ext-menus.js'
// gMaxLabelLength).
constexpr size_t kMaxSelectionLabelLength = 64;

// Firefox's label for `item` (ext-menus.js' customizeElement()): the title
// in full, with its access key (firefox_menu_access_keys.h), and with text
// selected, each %s replaced by the selection without its outer white space,
// cut so that the label is at most 64 characters, ending in "…"
// ("selection is: 'just some text 1234…'"). Without a selection %s stays.
// Chromium cuts the title at 75 characters and the selection at 24
// (PrintableSelectionText()), and replaces %s with nothing.
std::u16string FirefoxMenuItemTitle(const MenuItem& item,
                                    const std::u16string& selection_text) {
  FirefoxMenuLabel parsed =
      ParseFirefoxMenuTitle(base::UTF8ToUTF16(item.title()));
  std::u16string& label = parsed.label;
  if (selection_text.empty() || label.find(u"%s") == std::u16string::npos) {
    return FirefoxMenuTitle(label, parsed.access_key);
  }
  std::u16string_view selection =
      base::TrimWhitespace(selection_text, base::TRIM_ALL);
  std::u16string shown(selection);
  // As ext-menus.js' customizeElement() counts it: the label's UTF-16 units
  // without one %s, and the selection's code points.
  size_t length = label.size() - 2 + CodePointCount(selection);
  if (length > kMaxSelectionLabelLength) {
    size_t kept = CodePointCount(selection) -
                  std::min(CodePointCount(selection),
                           length - kMaxSelectionLabelLength + 1);
    shown = base::StrCat({FirstCodePoints(selection, kept), u"\u2026"});
  }
  base::ReplaceSubstringsAfterOffset(&label, 0, u"%s", shown);
  return FirefoxMenuTitle(label, parsed.access_key);
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
  // A password field, editable or not, as Firefox's onPassword. Chromium has
  // no password context; an editable one is also in the editable context.
  if (params.form_control_type ==
          blink::mojom::FormControlType::kInputPassword &&
      item.contexts().Contains(MenuItem::FIREFOX_PASSWORD)) {
    return true;
  }
  return ContextsMatch(params, item.contexts());
}

std::u16string ContextMenuMatcher::ItemTitle(
    const MenuItem* item,
    const std::u16string& selection_text) const {
  if (item->firefox_add_on()) {
    return FirefoxMenuItemTitle(*item, firefox_selection_text_);
  }
  return item->TitleWithReplacement(selection_text,
                                    kMaxExtensionItemTitleLength);
}

// Firefox's buildTopLevelElements() for an action menu: all the items at the
// top, where Chromium shows the first six and drops the others. Past
// ACTION_MENU_TOP_LEVEL_LIMIT, the limit's place is a submenu with the
// add-on's name that holds the rest. Firefox counts the items that show; the
// model has the hidden ones too. A separator follows, before the menu's own
// items, which FirefoxItemsAppended() puts after the add-on's.
bool ContextMenuMatcher::AppendFirefoxActionItems(
    const Extension* extension,
    const MenuItem::List& items,
    bool can_cross_incognito,
    const std::u16string& selection_text,
    int* index) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return false;
  }
  const size_t limit = api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT;
  size_t visible = std::ranges::count_if(
      items, [](const MenuItem* item) { return item->visible(); });
  MenuItem::List top_level;
  MenuItem::List overflow;
  size_t shown = 0;
  for (MenuItem* item : items) {
    if (visible > limit && shown == limit - 1) {
      overflow.push_back(item);
      continue;
    }
    top_level.push_back(item);
    if (item->visible()) {
      ++shown;
    }
  }

  RecursivelyAppendExtensionItems(top_level, can_cross_incognito,
                                  selection_text, menu_model_, index,
                                  /*is_action_menu_top_level=*/false);
  if (!overflow.empty()) {
    int menu_id = ConvertToExtensionsCustomCommandId(*index);
    ++(*index);
    auto submenu = std::make_unique<ui::SimpleMenuModel>(delegate_);
    RecursivelyAppendExtensionItems(overflow, can_cross_incognito,
                                    selection_text, submenu.get(), index,
                                    /*is_action_menu_top_level=*/false);
    // An & in the name is text, not an access key.
    std::u16string name = base::UTF8ToUTF16(extension->name());
    base::ReplaceSubstringsAfterOffset(&name, 0, u"&", u"&&");
    menu_model_->AddSubMenu(menu_id, name, submenu.get());
    extension_menu_models_.push_back(std::move(submenu));
  }
  // Only after an item that shows: appended after the menu's own items, at a
  // refresh(), the separator would otherwise come first once moved.
  if (visible) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  return true;
}

}  // namespace extensions
