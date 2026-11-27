// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_background.h"

#include <stddef.h>

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

constexpr char kBackground[] = "background";
constexpr char kPreferredEnvironment[] = "background.preferred_environment";

// Firefox's manifest warning (ExtensionData.manifestWarning()).
void Warn(Extension* extension, std::string_view key, std::string_view text) {
  extension->AddInstallWarning(InstallWarning(
      base::StrCat({"Reading manifest: Warning processing ", key, ": ", text}),
      key));
}

}  // namespace

bool PickFirefoxBackground(Extension* extension,
                           GURL& page,
                           std::vector<ExtensionResource>& scripts,
                           std::optional<GURL>& service_worker,
                           std::u16string* error) {
  const Manifest& manifest = *extension->manifest();

  // BackgroundInfo refused another type with a service worker already.
  const std::string* type =
      manifest.FindStringPath(manifest_keys::kBackgroundServiceWorkerType);
  if (type && *type != "classic" && *type != "module") {
    *error = base::UTF8ToUTF16(
        base::StrCat({"Reading manifest: Error processing background.type: "
                      "Invalid enumeration value \"",
                      *type, "\""}));
    return false;
  }

  // Firefox's schema drops an entry that it doesn't know, with a warning.
  const base::Value* preferred = manifest.FindPath(kPreferredEnvironment);
  std::optional<bool> worker_first;
  if (preferred && preferred->is_list()) {
    const base::ListValue& environments = preferred->GetList();
    for (size_t i = 0; i < environments.size(); ++i) {
      const std::string* environment = environments[i].GetIfString();
      if (!environment) {
        continue;
      }
      if (*environment == "document" || *environment == "service_worker") {
        if (!worker_first.has_value()) {
          worker_first = *environment == "service_worker";
        }
        continue;
      }
      Warn(extension, kPreferredEnvironment,
           base::StrCat({"Error processing ", kPreferredEnvironment, ".",
                         base::NumberToString(i),
                         ": Invalid enumeration value \"", *environment,
                         "\""}));
    }
  }

  if (page.is_valid() && !scripts.empty()) {
    Warn(extension, kBackground,
         "Both background.page and background.scripts specified. "
         "background.scripts will be ignored.");
    scripts.clear();
  }

  if (!service_worker.has_value() || (!page.is_valid() && scripts.empty())) {
    return true;
  }
  // Firefox takes "preferred_environment" as the author's choice.
  if (!preferred) {
    const char* document = page.is_valid() ? "page" : "scripts";
    Warn(extension, kBackground,
         base::StrCat({"with both background.service_worker and background.",
                       document, ", only background.", document,
                       " will be loaded. This can be changed with "
                       "background.preferred_environment."}));
  }
  if (worker_first.value_or(false)) {
    page = GURL();
    scripts.clear();
  } else {
    service_worker.reset();
  }
  return true;
}

}  // namespace extensions
