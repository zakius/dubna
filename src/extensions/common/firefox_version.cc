// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_version.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/version_name_info.h"

namespace extensions {

namespace {

// Firefox's ExtensionData.EXT_NAME_MAX_LEN.
constexpr size_t kFirefoxNameMaxLength = 75;

}  // namespace

base::Version LoadFirefoxVersion(const std::string& version,
                                 Extension* extension) {
  if (!IsFirefoxAddOn(*extension)) {
    return base::Version();
  }
  std::string_view numbers = std::string_view(version).substr(
      0, version.find_first_not_of("0123456789."));
  if (numbers.empty()) {
    return base::Version();
  }
  std::vector<uint32_t> components;
  for (std::string_view part : base::SplitStringPiece(
           numbers, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    unsigned number = 0;
    if (!part.empty() && !base::StringToUint(part, &number)) {
      return base::Version();
    }
    components.push_back(number);
  }
  base::Version parsed(std::move(components));
  // The version_name handler parses the manifest's own key later, and an
  // add-on has only one version name.
  if (parsed.GetString() != version &&
      !extension->manifest()->FindKey(manifest_keys::kVersionName)) {
    extension->SetManifestData(std::make_unique<VersionNameInfo>(version));
  }
  return parsed;
}

std::string LoadFirefoxName(const std::string& name,
                            const Extension& extension) {
  if (!IsFirefoxAddOn(extension)) {
    return name;
  }
  std::u16string utf16 = base::UTF8ToUTF16(name);
  if (utf16.size() <= kFirefoxNameMaxLength) {
    return name;
  }
  utf16.resize(kFirefoxNameMaxLength);
  return base::UTF16ToUTF8(utf16);
}

}  // namespace extensions
