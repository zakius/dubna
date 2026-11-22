// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_MENU_TARGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_MENU_TARGET_H_

namespace blink {

class Node;

// Firefox add-ons (dubna's webextensions/README.md): menus.getTargetElement,
// as Firefox's ContextMenuChild and child/ext-menus.js have it. Each window
// keeps the element of the last context menu that opened in it, weakly, with
// the menu's ID: the menus' targetElementId, which the browser passes on in
// onShown's and onClicked's info. WebElement::FirefoxMenuTarget() gives it
// back to a page or content script of that window that has the ID, while
// the element is in the window's document, also in a shadow tree. Another
// window, an ID of an older menu, and an element removed from the document
// get null.
//
// Records the element that a menu opens on as the last menu target of its
// window, and returns the menu's new ID. `node` is the node that the menu's
// hit test found, out of user agent shadow trees: an element, or a text node
// in one, as the contextmenu event's target is. Returns 0, no ID, for a
// node that is neither.
int SetFirefoxMenuTarget(Node* node);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_MENU_TARGET_H_
