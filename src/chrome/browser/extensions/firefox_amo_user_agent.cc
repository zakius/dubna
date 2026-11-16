// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_amo_user_agent.h"

#include "base/strings/strcat.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kAmoHost[] = "addons.mozilla.org";

// Firefox's own platform part of its user agent.
constexpr char kFirefoxPlatform[] =
#if BUILDFLAG(IS_WIN)
    "Windows NT 10.0; Win64; x64";
#elif BUILDFLAG(IS_MAC)
    "Macintosh; Intel Mac OS X 10.15";
#elif BUILDFLAG(IS_ANDROID)
    "Android 10; Mobile";
#else
    "X11; Linux x86_64";
#endif

bool IsAmo(const GURL& url) {
  return url.host() == kAmoHost;
}

// Ours, also one that an older version set and a restored session kept.
bool IsFirefoxUserAgent(const std::string& user_agent) {
  return user_agent.find(" Firefox/") != std::string::npos;
}

}  // namespace

std::string FirefoxUserAgent() {
  const std::string version = version_info::GetMajorVersionNumber() + ".0";
  // Firefox for Android puts its version where the desktop has a fixed date.
  const std::string gecko =
#if BUILDFLAG(IS_ANDROID)
      version;
#else
      "20100101";
#endif
  return base::StrCat({"Mozilla/5.0 (", kFirefoxPlatform, "; rv:", version,
                       ") Gecko/", gecko, " Firefox/", version});
}

FirefoxAmoUserAgent::FirefoxAmoUserAgent(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FirefoxAmoUserAgent>(*web_contents) {}

FirefoxAmoUserAgent::~FirefoxAmoUserAgent() = default;

void FirefoxAmoUserAgent::DidStartNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation->IsInPrimaryMainFrame() || navigation->IsSameDocument() ||
      navigation->IsPageActivation()) {
    return;
  }
  if (IsAmo(navigation->GetURL())) {
    // No metadata: Firefox sends no user agent client hints.
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override = FirefoxUserAgent();
    web_contents()->SetUserAgentOverride(ua_override, false);
    navigation->SetIsOverridingUserAgent(true);
  } else if (IsFirefoxUserAgent(
                 web_contents()->GetUserAgentOverride().ua_string_override)) {
    // A navigation inherits the override from the page it leaves.
    navigation->SetIsOverridingUserAgent(false);
  }
}

void FirefoxAmoUserAgent::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (!navigation->IsInPrimaryMainFrame() || !navigation->HasCommitted() ||
      navigation->IsSameDocument() || navigation->IsErrorPage() ||
      navigation->IsPost()) {
    return;
  }
  // The user agent is chosen when a navigation starts, so a redirect onto
  // AMO or away from it, or an activated prerender, can commit a page with
  // the other one. Such a page loads again, with the right one.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  const bool firefox =
      entry->GetIsOverridingUserAgent() &&
      IsFirefoxUserAgent(
          web_contents()->GetUserAgentOverride().ua_string_override);
  if (firefox != IsAmo(navigation->GetURL())) {
    web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                           false);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FirefoxAmoUserAgent);

}  // namespace extensions
