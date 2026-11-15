// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/theme/theme_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extensions_browser_client.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#endif

namespace extensions {

namespace {

constexpr char kOnUpdated[] = "theme.onUpdated";

#if !BUILDFLAG(IS_ANDROID)

// Firefox's theme keys and the browser colours closest to each.
struct FirefoxColor {
  const char* name;
  ui::ColorId id;
};
constexpr FirefoxColor kFirefoxColors[] = {
    {"frame", ui::kColorFrameActive},
    {"frame_inactive", ui::kColorFrameInactive},
    {"tab_background_text", kColorTabForegroundInactiveFrameActive},
    {"tab_background_separator", kColorTabDividerFrameActive},
    {"tab_selected", kColorTabBackgroundActiveFrameActive},
    {"tab_text", kColorTabForegroundActiveFrameActive},
    {"toolbar", kColorToolbar},
    {"toolbar_text", kColorToolbarText},
    {"toolbar_top_separator", kColorToolbarTopSeparatorFrameActive},
    {"icons", kColorToolbarButtonIcon},
    {"toolbar_field", kColorLocationBarBackground},
    {"toolbar_field_text", kColorOmniboxText},
    {"toolbar_field_border", kColorLocationBarBorder},
    {"popup", kColorOmniboxResultsBackground},
    {"popup_text", kColorOmniboxText},
    {"popup_highlight", kColorOmniboxResultsBackgroundSelected},
    {"popup_highlight_text", kColorOmniboxResultsTextSelected},
    {"sidebar", kColorSidePanelBackground},
    {"ntp_background", kColorNewTabPageBackground},
    {"ntp_text", kColorNewTabPageText},
};

// A colour as CSS: #rrggbb, or rgba() when it isn't opaque.
std::string CssColor(SkColor color) {
  if (SkColorGetA(color) == SK_AlphaOPAQUE) {
    return base::StringPrintf("#%02x%02x%02x", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color));
  }
  return base::StringPrintf("rgba(%d, %d, %d, %.3g)", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color),
                            SkColorGetA(color) / 255.0);
}

// The profile's last focused window, an incognito one too when the add-on may
// see it.
BrowserWindowInterface* LastActiveWindow(Profile& profile,
                                         bool include_incognito) {
  BrowserWindowInterface* found = nullptr;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* window) {
        const bool seen = include_incognito
                              ? profile.IsSameOrParent(window->GetProfile())
                              : window->GetProfile() == &profile;
        if (!seen) {
          return true;
        }
        found = window;
        return false;
      },
      BrowserCollection::Order::kActivation);
  return found;
}

// The colours of a window's theme, as its tabs get them; null, as in Firefox's
// empty theme, without a window or a tab.
base::Value Colors(BrowserWindowInterface* window) {
  if (!window || !window->GetActiveTabInterface()) {
    return base::Value();
  }
  base::DictValue colors;
  const ui::ColorProvider& provider =
      window->GetActiveTabInterface()->GetContents()->GetColorProvider();
  for (const FirefoxColor& color : kFirefoxColors) {
    colors.Set(color.name, CssColor(provider.GetColor(color.id)));
  }
  return base::Value(std::move(colors));
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Firefox's Theme. Its images and properties are null, as in Firefox's empty
// theme: a Chromium theme's images are its own, and it has no properties.
base::DictValue Theme(base::Value colors) {
  return base::DictValue()
      .Set("colors", std::move(colors))
      .Set("images", base::Value())
      .Set("properties", base::Value());
}

// The theme of the profile's last focused window.
base::DictValue CurrentTheme(Profile& profile, bool include_incognito) {
#if BUILDFLAG(IS_ANDROID)
  // Firefox's empty theme.
  return Theme(base::Value());
#else
  return Theme(Colors(LastActiveWindow(profile, include_incognito)));
#endif
}

}  // namespace

ThemeAPI::ThemeAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  observation_.Observe(ThemeServiceFactory::GetForProfile(profile_));
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

ThemeAPI::~ThemeAPI() = default;

// static
BrowserContextKeyedAPIFactory<ThemeAPI>* ThemeAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<ThemeAPI>> factory;
  return factory.get();
}

void ThemeAPI::OnThemeChanged() {
  ScheduleOnUpdated();
}

void ThemeAPI::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  ScheduleOnUpdated();
}

void ThemeAPI::ScheduleOnUpdated() {
  if (on_updated_scheduled_) {
    return;
  }
  on_updated_scheduled_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ThemeAPI::DispatchOnUpdated,
                                weak_factory_.GetWeakPtr()));
}

void ThemeAPI::DispatchOnUpdated() {
  on_updated_scheduled_ = false;
  // The theme of a regular window: the event reaches add-ons that may not see
  // incognito windows. Recorded with no listener too, so that a later change
  // back to it fires.
  base::DictValue theme = CurrentTheme(*profile_, /*include_incognito=*/false);
  if (theme == last_theme_) {
    return;
  }
  last_theme_ = theme.Clone();
  EventRouter* router = EventRouter::Get(profile_);
  if (!router || !router->HasEventListener(kOnUpdated)) {
    return;
  }
  base::ListValue args;
  args.Append(base::DictValue().Set("theme", std::move(theme)));
  // A stub histogram value: one of our own could clash with the upstreams'
  // additions to that enum, and UNKNOWN fails a DCHECK. Not restricted to the
  // regular profile, so that the incognito background of a split mode add-on
  // gets it too.
  router->BroadcastEvent(
      std::make_unique<Event>(events::FOR_TEST, kOnUpdated, std::move(args)));
}

template <>
void BrowserContextKeyedAPIFactory<ThemeAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ThemeServiceFactory::GetInstance());
}

ExtensionFunction::ResponseAction ThemeGetCurrentFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
#if BUILDFLAG(IS_ANDROID)
  return RespondNow(WithArguments(CurrentTheme(*profile, false)));
#else
  if (args().empty() || !args()[0].is_int()) {
    return RespondNow(WithArguments(
        CurrentTheme(*profile, include_incognito_information())));
  }
  std::string error;
  WindowController* controller = ExtensionTabUtil::GetControllerFromWindowID(
      ChromeExtensionFunctionDetails(this), args()[0].GetInt(), &error);
  if (!controller) {
    return RespondNow(Error(std::move(error)));
  }
  return RespondNow(WithArguments(
      Theme(Colors(controller->GetBrowserWindowInterface()))));
#endif
}

}  // namespace extensions
