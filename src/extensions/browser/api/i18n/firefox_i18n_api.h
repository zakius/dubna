// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_API_I18N_FIREFOX_I18N_API_H_
#define EXTENSIONS_BROWSER_API_I18N_FIREFOX_I18N_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

// Firefox's i18n.getPreferredSystemLanguages() (dubna's
// webextensions/README.md): the system's languages, as BCP 47 tags. It runs
// in the browser, as getAcceptLanguages() does, so that content scripts reach
// it too. Its histogram value is UNKNOWN, so that the upstreams' additions to
// that enum can't conflict with it.
class I18nGetPreferredSystemLanguagesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("i18n.getPreferredSystemLanguages", UNKNOWN)

 protected:
  ~I18nGetPreferredSystemLanguagesFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_I18N_FIREFOX_I18N_API_H_
