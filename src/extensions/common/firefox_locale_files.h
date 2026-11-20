// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_LOCALE_FILES_H_
#define EXTENSIONS_COMMON_FIREFOX_LOCALE_FILES_H_

#include <string>

namespace base {
class Value;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the text that a message's
// placeholder puts in for $name$, as Firefox's LocaleData.addLocale() reads it
// (ExtensionCommon.sys.mjs). A placeholder that isn't an object, or has no
// "content", gives nothing; a content that isn't a string gives the text
// JavaScript makes of it, so ["Accepted"] gives "Accepted". Chromium refuses
// to load an add-on with such a placeholder.
std::string FirefoxPlaceholderContent(const base::Value& placeholder);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_LOCALE_FILES_H_
