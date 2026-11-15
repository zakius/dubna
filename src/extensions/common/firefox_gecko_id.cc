// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_gecko_id.h"

#include <string>

#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"

namespace extensions {

namespace {

// Firefox's pattern for an add-on ID, case-insensitive:
// \{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\} or
// [a-z0-9-._]*@[a-z0-9-._]+
bool IsGeckoId(std::string_view id) {
  if (id.size() == 38 && id.front() == '{' && id.back() == '}') {
    for (size_t i = 1; i < 37; ++i) {
      const bool dash = i == 9 || i == 14 || i == 19 || i == 24;
      if (dash ? id[i] != '-' : !base::IsHexDigit(id[i])) {
        return false;
      }
    }
    return true;
  }
  const size_t at = id.find('@');
  if (at == std::string_view::npos || at + 1 == id.size()) {
    return false;
  }
  for (size_t i = 0; i < id.size(); ++i) {
    const char c = id[i];
    if (i != at && !base::IsAsciiAlphaNumeric(c) && c != '-' && c != '.' &&
        c != '_') {
      return false;
    }
  }
  return true;
}

}  // namespace

ExtensionId ExtensionIdFromGeckoId(std::string_view gecko_id) {
  if (!IsGeckoId(gecko_id)) {
    return ExtensionId();
  }
  return crx_file::id_util::GenerateId(gecko_id);
}

ExtensionId ExtensionIdFromManifest(const base::DictValue& manifest) {
  for (const char* path :
       {"browser_specific_settings.gecko.id", "applications.gecko.id"}) {
    if (const std::string* gecko_id = manifest.FindStringByDottedPath(path)) {
      return ExtensionIdFromGeckoId(*gecko_id);
    }
  }
  return ExtensionId();
}

}  // namespace extensions
