// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_add_on_availability.h"

#include <array>
#include <string_view>

#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "url/gurl.h"

namespace extensions::firefox_add_on_availability {

namespace {

// dubna's tools/firefox_gates.py checks that this list and the features with
// the flag are the same.
constexpr auto kFeatures = std::to_array<std::string_view>({
    "browserSettings",
    "contentScripts",
    "contextualIdentities",
    "menus",
    "pageAction",
    "runtime.getDocumentId",
    "scripting",
    "storage.session",
    "theme",
});

bool IsAvailable(std::string_view api_full_name,
                 const Extension* extension,
                 mojom::ContextType context,
                 const GURL& url,
                 Feature::Platform platform,
                 int context_id,
                 bool check_developer_mode,
                 const ContextData& context_data) {
  return extension && IsFirefoxAddOn(*extension);
}

}  // namespace

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap() {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  for (std::string_view feature : kFeatures) {
    map.emplace(feature, &IsAvailable);
  }
  return map;
}

}  // namespace extensions::firefox_add_on_availability
