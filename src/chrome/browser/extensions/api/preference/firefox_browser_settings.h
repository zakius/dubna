// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_FIREFOX_BROWSER_SETTINGS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_FIREFOX_BROWSER_SETTINGS_H_

#include <optional>
#include <string_view>

#include "base/values.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"

class Profile;

namespace extensions {

// Firefox's browserSettings, read-only. Each setting is a types.ChromeSetting
// whose key starts "browserSettings." (browser_settings.json), so its get(),
// set() and clear() reach the preference functions, which hand such a key to
// these instead of a pref. Firefox's privacy.network.peerConnectionEnabled
// (privacy.json) goes the same way, and is always true.
bool IsFirefoxBrowserSetting(std::string_view key);

// The permission that get() needs: privacy for the privacy setting,
// browserSettings for the others.
mojom::APIPermissionID FirefoxSettingPermission(std::string_view key);

// get()'s result, {value, levelOfControl}, with the value as Firefox gives
// it; none for a key that isn't a setting.
std::optional<base::DictValue> GetFirefoxBrowserSetting(Profile& profile,
                                                        std::string_view key);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_FIREFOX_BROWSER_SETTINGS_H_
