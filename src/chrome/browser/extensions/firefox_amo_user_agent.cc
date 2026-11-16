// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_amo_user_agent.h"

#include "base/strings/strcat.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
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
  const std::string firefox = FirefoxUserAgent();
  if (navigation->GetURL().host() == kAmoHost) {
    // No metadata: Firefox sends no user agent client hints.
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override = firefox;
    web_contents()->SetUserAgentOverride(ua_override, false);
    navigation->SetIsOverridingUserAgent(true);
  } else if (web_contents()->GetUserAgentOverride().ua_string_override ==
             firefox) {
    // A navigation inherits the override from the page it leaves.
    navigation->SetIsOverridingUserAgent(false);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FirefoxAmoUserAgent);

}  // namespace extensions
