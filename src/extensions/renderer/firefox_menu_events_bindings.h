// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_MENU_EVENTS_BINDINGS_H_
#define EXTENSIONS_RENDERER_FIREFOX_MENU_EVENTS_BINDINGS_H_

#include <string_view>

#include "v8/include/v8-forward.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): whether the bindings
// leave out `name`, an API's function, event or property, for `extension`
// (null in a context without one). contextMenus.onShown, onHidden and
// refresh() are Firefox's (firefox_menu_events.h), and only Firefox add-ons
// get the events and a refresh() that does something. Other extensions keep
// Chromium's contextMenus as it is, since one may tell Chrome from Firefox
// by `'onShown' in chrome.contextMenus`.
bool IsFirefoxMenusMemberHidden(const Extension* extension,
                                std::string_view name);

// Whether `context` is a Firefox add-on's, for the bindings, which don't see
// extensions (FirefoxUpdateFilters::SetAddOnCheck).
bool IsFirefoxAddOnContext(v8::Local<v8::Context> context);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_MENU_EVENTS_BINDINGS_H_
