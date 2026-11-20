// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_SELECTED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_SELECTED_TEXT_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;

// Firefox add-ons (dubna's webextensions/README.md): the text selected in
// `frame`, as Firefox's menus give it to add-ons (the selection context,
// selectionText and an item title's %s), wherever in the frame the menu
// opens. Chromium's context menu has the selection only on a click on it,
// for its own items. The text is the rendered one, as Firefox's
// Selection.toString() is, but a selection that starts at white space keeps
// it as one space, as Firefox's does (" just some text"), and it stops at
// 16384 characters, as Firefox's SelectionUtils.getSelectionDetails() does.
String FirefoxSelectedText(const LocalFrame& frame);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FIREFOX_SELECTED_TEXT_H_
