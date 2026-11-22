// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_LAST_ERROR_H_
#define EXTENSIONS_RENDERER_FIREFOX_LAST_ERROR_H_

#include <string_view>

#include "v8/include/v8-forward.h"

namespace extensions {

class ScriptContext;

// Firefox add-ons (dubna's webextensions/README.md): a Firefox add-on's
// runtime.lastError is null where there is no error, as Firefox's getter
// gives (ExtensionCommon.sys.mjs' BaseContext), and add-ons and Firefox's
// tests compare it with null. Chromium has no lastError property then, which
// reads undefined; `if (runtime.lastError)` takes both as no error.
//
// The null is a data property of an object between runtime and its
// prototype, so 'lastError' in runtime is true, as in Firefox. APILastError
// still puts an error's accessor on runtime itself and deletes it after the
// callback, which shows the null again. It reads a lastError that is null and
// not runtime's own as no error, where it would leave a value that script set.
// Other extensions keep Chromium's undefined.

// Adds the null under `binding`, the API object of `api_name`, if it is
// runtime in a Firefox add-on's page, worker or content script.
void AddFirefoxNullLastError(ScriptContext* script_context,
                             std::string_view api_name,
                             v8::Local<v8::Object> binding);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_LAST_ERROR_H_
