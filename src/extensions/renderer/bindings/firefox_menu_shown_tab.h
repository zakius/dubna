// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_BINDINGS_FIREFOX_MENU_SHOWN_TAB_H_
#define EXTENSIONS_RENDERER_BINDINGS_FIREFOX_MENU_SHOWN_TAB_H_

#include <string_view>

#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): menus.onShown's
// arguments, which `args` has as the browser sent them. Firefox calls a
// listener with the info and the tab, which is undefined in a menu without
// one, such as an add-on popup's: the listener sees two arguments. The
// browser can't send undefined (a base::Value converts to null), so it
// sends the info alone (firefox_menu_events.cc), and this adds the
// undefined. Only Firefox add-ons get onShown.
void AddFirefoxMenuShownTab(std::string_view event_name,
                            v8::LocalVector<v8::Value>& args);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_MENU_SHOWN_TAB_H_
