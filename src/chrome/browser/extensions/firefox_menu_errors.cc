// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_errors.h"

#include "base/strings/string_util.h"
#include "chrome/browser/extensions/firefox_menu_store.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

namespace {

// From a persistent background, MenuItem's id setter refuses the id.
constexpr char kDuplicateIdError[] = "ID already exists: *";
// From one that isn't, menusInternal.create refuses it first
// (ExtensionMenus.shouldPersistMenus()).
constexpr char kNonPersistentDuplicateIdError[] =
    "The menu id * already exists in menus.create.";
constexpr char kParentError[] =
    "MenuItem cannot be an ancestor (or self) of its new parent.";

}  // namespace

void SetFirefoxDuplicateMenuIdError(const Extension* extension,
                                    std::string_view id,
                                    std::string* error) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  // The backgrounds whose items need an id (ContextMenusCreateFunction).
  bool persistent = !BackgroundInfo::HasLazyContext(extension) &&
                    !KeepsFirefoxMenus(*extension);
  *error = ErrorUtils::FormatErrorMessage(
      persistent ? kDuplicateIdError : kNonPersistentDuplicateIdError, id);
}

void SetFirefoxMenuParentError(const Extension* extension, std::string* error) {
  if (extension && IsFirefoxAddOn(*extension) && error->empty()) {
    *error = kParentError;
  }
}

void SetFirefoxUrlPatternError(const Extension* extension, std::string* error) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  // Chromium's text is its format with the pattern in place of the *.
  std::string_view format = manifest_errors::kInvalidURLPatternError;
  size_t star = format.find('*');
  std::string_view prefix = format.substr(0, star);
  std::string_view suffix = format.substr(star + 1);
  std::string_view text = *error;
  if (text.size() < prefix.size() + suffix.size() ||
      !base::StartsWith(text, prefix) || !base::EndsWith(text, suffix)) {
    return;
  }
  std::string_view pattern =
      text.substr(prefix.size(), text.size() - prefix.size() - suffix.size());
  *error =
      ErrorUtils::FormatErrorMessage(kFirefoxInvalidUrlPatternError, pattern);
}

}  // namespace extensions
