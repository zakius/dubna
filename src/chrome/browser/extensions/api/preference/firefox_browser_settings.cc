// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/preference/firefox_browser_settings.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/accessibility/page_colors_controller.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr std::string_view kPrefix = "browserSettings.";
constexpr std::string_view kPeerConnectionEnabled =
    "privacy.network.peerConnectionEnabled";

// oganesson's tab behavior prefs (its docs/tab-behavior.md), its equals of
// Firefox's browser.tabs.insertAfterCurrent and insertRelatedAfterCurrent.
// Browsers that don't register them open tabs as Chromium does.
constexpr char kNewTabNextToActive[] = "helium.browser.new_tab_next_to_active";
constexpr char kTabLinkPosition[] = "oganesson.tabs.link_position";
constexpr int kTabLinksAtEnd = 2;

std::string NewTabPosition(const PrefService& prefs) {
  const PrefService::Preference* next_to_active =
      prefs.FindPreference(kNewTabNextToActive);
  if (next_to_active &&
      next_to_active->GetValue()->GetIfBool().value_or(false)) {
    return "afterCurrent";
  }
  const PrefService::Preference* links = prefs.FindPreference(kTabLinkPosition);
  if (links && links->GetValue()->GetIfInt() == kTabLinksAtEnd) {
    return "atEnd";
  }
  return "relatedAfterCurrent";
}

// Chromium's accessibilityFeatures.animationPolicy ("allowed", "once" or
// "none"), which desktop Android doesn't register.
std::string ImageAnimationBehavior(const PrefService& prefs) {
  const PrefService::Preference* policy =
      prefs.FindPreference(prefs::kAnimationPolicy);
  if (policy) {
    const std::string& value = policy->GetValue()->GetString();
    if (value == "once" || value == "none") {
      return value;
    }
  }
  return "normal";
}

// Chromium's page colors, which desktop Android doesn't register: forced
// colors follow the system's high contrast theme, or never or always apply.
std::string OverrideDocumentColors(const PrefService& prefs) {
  const PrefService::Preference* page_colors =
      prefs.FindPreference(prefs::kRequestedPageColors);
  const int requested =
      page_colors ? page_colors->GetValue()->GetInt()
                  : std::to_underlying(PageColors::kNoPreference);
  if (requested == std::to_underlying(PageColors::kOff)) {
    return "never";
  }
  if (requested == std::to_underlying(PageColors::kNoPreference) ||
      prefs.GetBoolean(prefs::kApplyPageColorsOnlyOnIncreasedContrast)) {
    return "high-contrast-only";
  }
  return "always";
}

// The value of the setting, its key without kPrefix; none for a key that isn't
// a setting.
std::optional<base::Value> Value(Profile& profile, std::string_view key) {
  const PrefService& prefs = *profile.GetPrefs();

  if (key == "homepageOverride") {
    // On Android, the user's home page is in Java (HomepageManager), and this
    // is the policy's or the new tab page: zakius/tennessine#44.
    return base::Value(profile.GetHomePage().spec());
  }
  if (key == "newTabPageOverride") {
    // An add-on's chrome_url_overrides page, or the browser's own.
    GURL new_tab(chrome::kChromeUINewTabURL);
    ExtensionUrlOverrides::HandleChromeURLOverride(&new_tab, &profile);
    return base::Value(new_tab.spec());
  }
  if (key == "newTabPosition") {
    return base::Value(NewTabPosition(prefs));
  }
  if (key == "imageAnimationBehavior") {
    return base::Value(ImageAnimationBehavior(prefs));
  }
  if (key == "webNotificationsDisabled") {
    return base::Value(
        HostContentSettingsMapFactory::GetForProfile(&profile)
            ->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS) ==
        CONTENT_SETTING_BLOCK);
  }
  if (key == "overrideDocumentColors") {
    return base::Value(OverrideDocumentColors(prefs));
  }
  if (key == "contextMenuShowEvent") {
    // Blink's WebPreferences::context_menu_on_mouse_up.
#if BUILDFLAG(IS_WIN)
    return base::Value("mouseup");
#else
    return base::Value("mousedown");
#endif
  }

  // What Chromium always does.
  if (key == "allowPopupsForUserEvents") {
    // A click may open a pop-up, which the pop-up blocker lets through.
    return base::Value(true);
  }
  if (key == "cacheEnabled" || key == "useDocumentFonts" ||
      key == "zoomFullPage" || key == "zoomSiteSpecific") {
    return base::Value(true);
  }
  if (key == "closeTabsByDoubleClick" || key == "ftpProtocolEnabled" ||
      key == "openBookmarksInNewTabs" || key == "openSearchResultsInNewTabs" ||
      key == "openUrlbarResultsInNewTabs") {
    return base::Value(false);
  }
  if (key == "overrideContentColorScheme") {
    // Pages follow the browser's light or dark mode.
    return base::Value("auto");
  }
  return std::nullopt;
}

}  // namespace

bool IsFirefoxBrowserSetting(std::string_view key) {
  return key.starts_with(kPrefix) || key == kPeerConnectionEnabled;
}

mojom::APIPermissionID FirefoxSettingPermission(std::string_view key) {
  if (key == kPeerConnectionEnabled) {
    return mojom::APIPermissionID::kPrivacy;
  }
  return mojom::APIPermissionID::kBrowserSettings;
}

std::optional<base::DictValue> GetFirefoxBrowserSetting(Profile& profile,
                                                        std::string_view key) {
  std::optional<base::Value> value;
  if (key == kPeerConnectionEnabled) {
    // WebRTC is on: turning it off is a later step.
    value = base::Value(true);
  } else if (key.starts_with(kPrefix)) {
    value = Value(profile, key.substr(kPrefix.size()));
  }
  if (!value) {
    return std::nullopt;
  }
  return base::DictValue()
      .Set("value", std::move(*value))
      .Set("levelOfControl", "not_controllable");
}

}  // namespace extensions
