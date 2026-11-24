// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_add_on.h"

#include <string_view>

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_gecko_id.h"
#include "extensions/common/manifest.h"

namespace extensions {

bool IsFirefoxAddOn(const Extension& extension) {
  const base::DictValue* manifest = extension.manifest()->value();
  if (Manifest::IsUnpackedLocation(extension.location())) {
    return manifest->FindDictByDottedPath("browser_specific_settings.gecko") ||
           manifest->FindDictByDottedPath("applications.gecko");
  }
  std::string_view gecko_id = GeckoId(*manifest);
  return !gecko_id.empty() &&
         extension.id() == ExtensionIdFromGeckoId(gecko_id);
}

}  // namespace extensions
