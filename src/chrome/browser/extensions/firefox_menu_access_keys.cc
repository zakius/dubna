// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_access_keys.h"

#include <optional>
#include <set>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "ui/base/models/menu_model.h"
#include "ui/menus/simple_menu_model.h"

namespace extensions {

namespace {

std::u16string Escaped(std::u16string_view label) {
  std::u16string escaped(label);
  base::ReplaceSubstringsAfterOffset(&escaped, 0, u"&", u"&&");
  return escaped;
}

// The access key that a Chromium menu title marks: the character after its
// first & that isn't part of an "&&". Empty when it marks none.
std::u16string ChromiumAccessKey(std::u16string_view title) {
  for (size_t i = 0; i + 1 < title.size(); ++i) {
    if (title[i] != u'&') {
      continue;
    }
    if (title[i + 1] != u'&') {
      bool pair = U16_IS_LEAD(title[i + 1]) && i + 2 < title.size();
      return std::u16string(title.substr(i + 1, pair ? 2 : 1));
    }
    ++i;
  }
  return std::u16string();
}

// auto-accesskey.mjs' TYPEABLE_CHARACTER: a letter or decimal digit that a
// single key press types. Not one of a script typed through an IME, nor a
// fullwidth form.
bool IsTypeable(UChar32 c) {
  if (c >= 0xFF00 && c <= 0xFFEF) {
    return false;
  }
  UErrorCode status = U_ZERO_ERROR;
  switch (uscript_getScript(c, &status)) {
    case USCRIPT_HAN:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_HANGUL:
    case USCRIPT_BOPOMOFO:
      return false;
    default:
      break;
  }
  switch (u_charType(c)) {
    case U_UPPERCASE_LETTER:
    case U_LOWERCASE_LETTER:
    case U_TITLECASE_LETTER:
    case U_OTHER_LETTER:
    case U_DECIMAL_DIGIT_NUMBER:
      return true;
    default:
      return false;
  }
}

std::u16string FirstTypeable(std::u16string_view label) {
  for (size_t i = 0; i < label.size(); ++i) {
    size_t start = i;
    base_icu::UChar32 c;
    if (base::ReadUnicodeCharacter(label, &i, &c) && IsTypeable(c)) {
      return std::u16string(label.substr(start, i + 1 - start));
    }
  }
  return std::u16string();
}

// The item of `model`'s entry at `index`, or for the entry of an add-on's
// several items, its submenu's first. Null for an entry of none.
const MenuItem* EntryItem(const ui::MenuModel& model,
                          size_t index,
                          const std::map<int, MenuItem::Id>& items,
                          const MenuManager& menu_manager) {
  int command_id = model.GetCommandIdAt(index);
  auto found = items.find(command_id);
  if (found != items.end()) {
    return menu_manager.GetItemById(found->second);
  }
  ui::MenuModel* submenu = model.GetSubmenuModelAt(index);
  if (!submenu ||
      !ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return nullptr;
  }
  for (size_t i = 0; i < submenu->GetItemCount(); ++i) {
    if (const MenuItem* item = EntryItem(*submenu, i, items, menu_manager)) {
      return item;
    }
  }
  return nullptr;
}

}  // namespace

FirefoxMenuLabel ParseFirefoxMenuTitle(std::u16string_view title) {
  FirefoxMenuLabel parsed;
  // Unset until the first &, as ext-menus.js' accessKey is undefined.
  std::optional<std::u16string> access_key;
  for (size_t i = 0; i < title.size(); ++i) {
    if (title[i] != u'&') {
      parsed.label += title[i];
      continue;
    }
    if (i + 1 == title.size()) {
      access_key = access_key.value_or(std::u16string());
      break;
    }
    char16_t next = title[++i];
    parsed.label += next;
    if (next == u'&' || access_key) {
      continue;
    }
    bool before_selection =
        next == u'%' && i + 1 < title.size() && title[i + 1] == u's';
    access_key = before_selection ? std::u16string() : std::u16string(1, next);
  }
  parsed.access_key = access_key.value_or(std::u16string());
  return parsed;
}

std::u16string FirefoxMenuTitle(std::u16string_view label,
                                std::u16string_view access_key) {
  if (access_key.empty()) {
    return Escaped(label);
  }
  size_t at = label.find(access_key);
  if (at == std::u16string_view::npos) {
    return base::StrCat({Escaped(label), u" (&", access_key, u")"});
  }
  return base::StrCat(
      {Escaped(label.substr(0, at)), u"&", Escaped(label.substr(at))});
}

void AssignFirefoxAccessKeys(ui::SimpleMenuModel& model,
                             const std::map<int, MenuItem::Id>& items,
                             const MenuManager& menu_manager) {
  // auto-accesskey.mjs' assign(): the add-on's entries without an access key
  // are those it gives one; the others' keys are taken.
  std::vector<size_t> without_key;
  std::set<std::u16string> used_keys;
  bool has_others = false;
  bool others_have_keys = false;
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      continue;
    }
    std::u16string key = ChromiumAccessKey(model.GetLabelAt(i));
    const MenuItem* item = EntryItem(model, i, items, menu_manager);
    if (item && item->firefox_add_on() && key.empty()) {
      without_key.push_back(i);
      continue;
    }
    has_others = true;
    if (!key.empty()) {
      others_have_keys = true;
      used_keys.insert(key);
    }
  }
  if (has_others && !others_have_keys) {
    return;
  }

  std::vector<size_t> to_number;
  for (size_t i : without_key) {
    std::u16string label = model.GetLabelAt(i);
    base::ReplaceSubstringsAfterOffset(&label, 0, u"&&", u"&");
    std::u16string key = FirstTypeable(label);
    if (!key.empty()) {
      model.SetLabel(i, FirefoxMenuTitle(label, key));
      used_keys.insert(key);
    } else if (!base::TrimWhitespace(label, base::TRIM_ALL).empty()) {
      to_number.push_back(i);
    }
  }

  // The digits that no other entry uses, in keyboard order, in turn.
  std::vector<std::u16string> digits;
  for (char16_t digit : std::u16string_view(u"1234567890")) {
    std::u16string key(1, digit);
    if (!used_keys.contains(key)) {
      digits.push_back(key);
    }
  }
  if (digits.empty()) {
    return;
  }
  for (size_t n = 0; n < to_number.size(); ++n) {
    size_t i = to_number[n];
    std::u16string label = model.GetLabelAt(i);
    base::ReplaceSubstringsAfterOffset(&label, 0, u"&&", u"&");
    model.SetLabel(i, FirefoxMenuTitle(label, digits[n % digits.size()]));
  }
}

}  // namespace extensions
