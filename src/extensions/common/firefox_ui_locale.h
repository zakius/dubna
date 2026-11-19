// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_UI_LOCALE_H_
#define EXTENSIONS_COMMON_FIREFOX_UI_LOCALE_H_

#include "extensions/common/message_bundle.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's @@ui_locale is
// the browser's language as a BCP 47 tag ("en-US"), what i18n.getUILanguage()
// gives, and every add-on has it, with or without _locales. Chromium's is
// ICU's legacy form ("en_US"), and only an extension with _locales has it.
// For a Firefox add-on (firefox_add_on.h), this sets Firefox's in `messages`,
// the substitutions that getMessage() and the localization of CSS read.
void SetFirefoxUiLocale(MessageBundle::SubstitutionMap& messages);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_UI_LOCALE_H_
