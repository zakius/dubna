// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_API_FIREFOX_MENU_ITEMS_ONCLICK_H_
#define EXTENSIONS_RENDERER_API_FIREFOX_MENU_ITEMS_ONCLICK_H_

#include "v8/include/v8-forward.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): a menus item's onclick
// belongs to the item, as in Firefox, not to the page that set it. Each page
// keeps the handlers it set (context_menus_handlers.js), and these natives
// record which page set an item's onclick last, so that only that page runs
// it. So setting it from another page replaces the first page's, and
// removing the item from any page drops it. Chromium runs the handler of
// every page that set one, until that page removes the item itself. Items
// are keyed by context_menus_handlers.js ("string:id" or "number:id"), per
// add-on. For an extension that isn't a Firefox add-on, every page that set
// a handler runs it, as in Chromium.

// SetFirefoxOnclickPage(item, hasOnclick): the calling page set the item's
// onclick (hasOnclick), or the item has none any more.
void SetFirefoxOnclickPage(const v8::FunctionCallbackInfo<v8::Value>& args);

// IsFirefoxOnclickPage(item): whether the calling page runs the item's
// onclick.
void IsFirefoxOnclickPage(const v8::FunctionCallbackInfo<v8::Value>& args);

// ClearFirefoxOnclickPages(): the add-on's items have no onclick any more
// (menus.removeAll()).
void ClearFirefoxOnclickPages(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_FIREFOX_MENU_ITEMS_ONCLICK_H_
