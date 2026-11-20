// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_API_FIREFOX_I18N_SUBSTITUTIONS_H_
#define EXTENSIONS_RENDERER_API_FIREFOX_I18N_SUBSTITUTIONS_H_

#include <string>

#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): `message` with the
// `substitutions` of i18n.getMessage() put in as Firefox's
// LocaleData.localizeMessage() does (ExtensionCommon.sys.mjs). $ and a number
// of any length from 1 is that substitution, or nothing when there is none or
// it's a hole in the array; a run of $ loses its first; any other $ stays, as
// in "$0" or "$name". Substitutions that aren't an array are the only one.
// Chromium takes at most 9 substitutions, only one digit after $, and drops
// $ with the character after it, so "$foo" became "oo".
std::string FirefoxI18nSubstitutions(const std::string& message,
                                     v8::Local<v8::Value> substitutions,
                                     v8::Local<v8::Context> context);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_FIREFOX_I18N_SUBSTITUTIONS_H_
