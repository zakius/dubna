// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_STORAGE_H_
#define EXTENSIONS_RENDERER_FIREFOX_STORAGE_H_

#include <string_view>

#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8-forward.h"

namespace base {
class Value;
}

namespace extensions {

class ScriptContext;

// Firefox add-ons (dubna's webextensions/README.md): Firefox gives content
// scripts no storage.session, so `typeof browser.storage.session` is
// "undefined" there. Chromium's is there, and its calls fail until the
// background page calls setAccessLevel(), which Firefox doesn't have. True
// for that feature in a Firefox add-on's content script: the bindings leave
// it out, as they leave out a feature the context doesn't have.
bool IsHiddenFromFirefoxAddOn(const ScriptContext& script_context,
                              std::string_view feature_name);

// Firefox add-ons: storage.*.get() with an object reads each of the object's
// keys, as in Firefox, also one whose default is undefined. Chromium's
// bindings drop such a key on its way to the browser, so get({key:
// undefined}) read nothing. For a Firefox add-on's object with such a key,
// this replaces `keys`, the object as the bindings parsed it, with the list
// of all its keys, and returns the result modifier that puts in the result
// the defaults of the keys the area doesn't have, undefined for those
// without one. For any other call it leaves `keys` as it is and returns a
// null modifier. `arguments` are get()'s arguments as the add-on passed them.
binding::ResultModifierFunction FirefoxStorageGetDefaults(
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& arguments,
    base::Value& keys);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_STORAGE_H_
