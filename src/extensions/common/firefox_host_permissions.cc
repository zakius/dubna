// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_host_permissions.h"

#include <string_view>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

// As Firefox's classifyPermission(): a permission's name is words of letters,
// digits and _, joined by dots. Anything else is a host pattern, valid or not.
bool IsPermissionName(std::string_view value) {
  for (std::string_view word : base::SplitStringPiece(
           value, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (word.empty()) {
      return false;
    }
    for (char c : word) {
      if (!base::IsAsciiAlphaNumeric(c) && c != '_') {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

std::vector<std::string> TakeFirefoxHostPatterns(
    const Extension& extension,
    std::string_view key,
    std::vector<std::string>* unhandled) {
  std::vector<std::string> patterns;
  if (key != manifest_keys::kOptionalPermissions ||
      !IsFirefoxAddOn(extension)) {
    return patterns;
  }
  std::vector<std::string> names;
  for (std::string& value : *unhandled) {
    if (IsPermissionName(value)) {
      names.push_back(std::move(value));
    } else {
      patterns.push_back(std::move(value));
    }
  }
  *unhandled = std::move(names);
  return patterns;
}

}  // namespace extensions
