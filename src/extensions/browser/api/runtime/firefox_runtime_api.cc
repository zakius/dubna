// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/api/runtime/firefox_runtime_api.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "base/values.h"
#include "base/version_info/version_info.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// The Gecko IDs of the add-ons that work only when told they run in Firefox,
// as corpus runs find them (dubna's webextensions/README.md). None yet.
constexpr std::array<std::string_view, 0> kNamedFirefox = {};

// Whether `extension` is told it runs in Firefox.
bool NamedFirefox(const Extension& extension, const PrefService& prefs) {
  const base::DictValue& manifest = *extension.manifest()->value();
  const std::string* gecko_id =
      manifest.FindStringByDottedPath("browser_specific_settings.gecko.id");
  if (!gecko_id) {
    gecko_id = manifest.FindStringByDottedPath("applications.gecko.id");
  }
  if (gecko_id && std::ranges::find(kNamedFirefox, *gecko_id) !=
                      kNamedFirefox.end()) {
    return true;
  }
  for (const base::Value& entry : prefs.GetList(kFirefoxNamedAddOns)) {
    if (entry.is_string() &&
        (entry.GetString() == extension.id() ||
         (gecko_id && entry.GetString() == *gecko_id))) {
      return true;
    }
  }
  return false;
}

}  // namespace

void RegisterFirefoxRuntimePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kFirefoxNamedAddOns);
}

ExtensionFunction::ResponseAction RuntimeGetBrowserInfoFunction::Run() {
  // Add-ons take the function's presence as "this is Firefox", and compare
  // the version with Firefox versions that added an API ("version >= 53"):
  // Chromium's version is past them, so they take the paths for a current
  // Firefox. `name` is the browser's own, not "Firefox": add-ons compare it
  // with "Firefox" mostly to work around Firefox's bugs or to link to
  // Firefox's help, which would be wrong here. The add-ons that work only
  // when named Firefox are listed, and get Firefox's name and vendor.
  api::runtime::GetBrowserInfo::Results::BrowserInfo info;
  if (NamedFirefox(*extension(),
                   *user_prefs::UserPrefs::Get(browser_context()))) {
    info.name = "Firefox";
    info.vendor = "Mozilla";
  } else {
    info.name = std::string(version_info::GetProductName());
    info.vendor = info.name;
  }
  info.version = std::string(version_info::GetVersionNumber());
  info.build_id = info.version;
  return RespondNow(
      ArgumentList(api::runtime::GetBrowserInfo::Results::Create(info)));
}

}  // namespace extensions
