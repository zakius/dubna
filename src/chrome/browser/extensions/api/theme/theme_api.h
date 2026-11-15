// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_THEME_THEME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_THEME_THEME_API_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's theme API, read
// only. Fires theme.onUpdated when the browser's theme changes: the browser's
// own (ThemeService) or the system's (light or dark, a GTK theme), which the
// browser follows.
class ThemeAPI : public BrowserContextKeyedAPI,
                 public ThemeServiceObserver,
                 public ui::NativeThemeObserver {
 public:
  explicit ThemeAPI(content::BrowserContext* context);
  ThemeAPI(const ThemeAPI&) = delete;
  ThemeAPI& operator=(const ThemeAPI&) = delete;
  ~ThemeAPI() override;

  static BrowserContextKeyedAPIFactory<ThemeAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<ThemeAPI>;

  static const char* service_name() { return "ThemeAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Dispatches onUpdated once the windows have their new colours: they take
  // them from their own observers, which may run after these. One change can
  // reach both observers, and gives one event; a notice that changes no colour
  // gives none.
  void ScheduleOnUpdated();
  void DispatchOnUpdated();

  raw_ptr<Profile> profile_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver> observation_{
      this};
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  bool on_updated_scheduled_ = false;
  // The theme at the last change, which the last onUpdated gave if anyone
  // listened.
  std::optional<base::DictValue> last_theme_;
  base::WeakPtrFactory<ThemeAPI> weak_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<ThemeAPI>::DeclareFactoryDependencies();

// Its histogram value is UNKNOWN, so that the upstreams' additions to that
// enum can't conflict with it.
class ThemeGetCurrentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("theme.getCurrent", UNKNOWN)

 protected:
  ~ThemeGetCurrentFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_THEME_THEME_API_H_
