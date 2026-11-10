// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_FIREFOX_RUNTIME_API_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_FIREFOX_RUNTIME_API_H_

#include "extensions/browser/extension_function.h"

class PrefRegistrySimple;

namespace extensions {

// The add-ons that runtime.getBrowserInfo() tells they run in Firefox, by
// Gecko ID or extension ID, besides the ones dubna lists: a profile pref,
// edited by hand when an add-on works only so.
inline constexpr char kFirefoxNamedAddOns[] = "firefox_add_ons.named_firefox";

void RegisterFirefoxRuntimePrefs(PrefRegistrySimple* registry);

// Firefox's runtime.getBrowserInfo(). Its histogram value is UNKNOWN, so that
// the upstreams' additions to that enum can't conflict with it.
class RuntimeGetBrowserInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getBrowserInfo", UNKNOWN)

 protected:
  ~RuntimeGetBrowserInfoFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_FIREFOX_RUNTIME_API_H_
