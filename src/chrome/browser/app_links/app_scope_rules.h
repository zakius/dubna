// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_APP_LINKS_APP_SCOPE_RULES_H_
#define CHROME_BROWSER_APP_LINKS_APP_SCOPE_RULES_H_

#include <optional>
#include <string>
#include <string_view>

class GURL;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// The scope rules a user adds to an installed web app, by app id: each browser
// has its own ids (desktop's web app ids, Android's WebappDataStorage ids).
namespace app_links {

// Profile pref: per app id, the list of scope rules the user added to the
// app. A rule is a URLPattern constructor string, such as
// "https://*.example.com/*"; one starting with "!" takes the URLs it matches
// out of the scope, the manifest's included. The name is oganesson's, from
// before the rules were shared.
inline constexpr char kUserAppScopeRules[] =
    "oganesson.web_apps.user_scope_rules";

// Profile pref: per app id, true where the user made the app's scope strict:
// every page out of it leaves the app's window by any route, as an exclusion
// does (app_links.h).
inline constexpr char kUserAppStrictScope[] =
    "oganesson.web_apps.strict_scope";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Whether `url` is in the scope of app `app_id` as the user's rules decide, or
// nullopt where the manifest's scope decides. Include rules add to the
// manifest's scope, except on a host one of them names: there they replace it.
std::optional<bool> UserAppScopeOverride(const PrefService& prefs,
                                         std::string_view app_id,
                                         const GURL& url);

// App `app_id`'s score for `url` with its rules applied, given its score for
// the manifest's scope, as desktop's WebAppRegistrar ranks apps by: 0 if the
// rules take `url` out, `manifest_score` if it is in the manifest's scope, and
// 1, below any manifest scope, if only an include rule has it.
int GetUserAppScopeScore(const PrefService& prefs,
                         std::string_view app_id,
                         const GURL& url,
                         int manifest_score);

// Whether one of app `app_id`'s exclusions ("!" rules) matches `url`. A page
// in the app's window that reaches such a URL, by any route, is taken out of
// the window (app_links.h).
bool IsUrlExcludedFromUserAppScope(const PrefService& prefs,
                                   std::string_view app_id,
                                   const GURL& url);

// Whether the user made app `app_id`'s scope strict.
bool IsUserAppScopeStrict(const PrefService& prefs, std::string_view app_id);

void SetUserAppScopeStrict(PrefService& prefs,
                           std::string_view app_id,
                           bool strict);

// The app's rules, one per line.
std::string GetUserAppScopeRules(const PrefService& prefs,
                                 std::string_view app_id);

// Sets the app's rules from `rules`, one per line; blank lines are skipped.
// If a rule is invalid, keeps the rules as they were and returns a message
// naming it; otherwise returns an empty string.
std::string SetUserAppScopeRules(PrefService& prefs,
                                 std::string_view app_id,
                                 std::string_view rules);

}  // namespace app_links

#endif  // CHROME_BROWSER_APP_LINKS_APP_SCOPE_RULES_H_
