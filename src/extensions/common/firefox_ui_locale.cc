// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_ui_locale.h"

#include "base/i18n/rtl.h"

namespace extensions {

void SetFirefoxUiLocale(MessageBundle::SubstitutionMap& messages) {
  // The renderer's i18n.getUILanguage() reads the same function, and the
  // browser's ICU default locale is the same application locale.
  messages[MessageBundle::kUILocaleKey] = base::i18n::GetConfiguredLocale();
}

}  // namespace extensions
