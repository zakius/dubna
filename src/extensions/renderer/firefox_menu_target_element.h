// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_MENU_TARGET_ELEMENT_H_
#define EXTENSIONS_RENDERER_FIREFOX_MENU_TARGET_ELEMENT_H_

#include <string_view>

#include "v8/include/v8-forward.h"

namespace extensions {

class ScriptContext;

// Firefox add-ons (dubna's webextensions/README.md): menus.getTargetElement
// (targetElementId), as Firefox's child/ext-menus.js has it: the element
// that the last context menu of the calling page's or content script's own
// window opened on, while it is in that window's document, else null
// (blink's WebElement::FirefoxMenuTarget(), firefox_menu_target.h). So a
// page that calls it on another page's browser.menus reads that page's. As
// in Firefox, only a Firefox add-on with the menus permission has it
// (firefox_menus_permission.h), not one with contextMenus alone. Other
// extensions keep Chromium's contextMenus, which has no getTargetElement.

// In an add-on's page, getTargetElement() goes on `binding`, the API object
// of `api_name`, if it is contextMenus, which browser.menus is too.
void AddFirefoxGetTargetElement(ScriptContext* script_context,
                                std::string_view api_name,
                                v8::Local<v8::Object> binding);

// In a content script, which Chromium gives no contextMenus, browser.menus
// and chrome.menus are an object with getTargetElement() alone, as Firefox's
// menus are in content scripts.
void AddFirefoxContentScriptMenus(ScriptContext* script_context);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_MENU_TARGET_ELEMENT_H_
