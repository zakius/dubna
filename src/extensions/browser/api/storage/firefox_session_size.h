// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_API_STORAGE_FIREFOX_SESSION_SIZE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_FIREFOX_SESSION_SIZE_H_

#include <stddef.h>

#include <string_view>

namespace base {
class Value;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the bytes that Firefox's
// storage.session counts for an item, in getBytesInUse() and against its
// quota: the key's length in JavaScript characters and the size of the
// value's structured clone, the format Firefox keeps it in. Chromium counts
// only the memory a value takes outside its own object, so a short string, a
// number or a key counted 0 bytes. The value here is the one this browser
// keeps, whose structured clone Firefox's matches for JSON's types.
size_t FirefoxSessionItemSize(std::string_view key, const base::Value& value);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_FIREFOX_SESSION_SIZE_H_
