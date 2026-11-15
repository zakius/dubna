// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_THEME_THEME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_THEME_THEME_API_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's theme API, read
// only. Fires theme.onUpdated when the browser's theme changes.
class ThemeAPI : public BrowserContextKeyedAPI, public ThemeServiceObserver {
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

  void DispatchOnUpdated();

  raw_ptr<Profile> profile_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver> observation_{
      this};
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
