// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_BROWSER_STYLE_H_
#define EXTENSIONS_RENDERER_FIREFOX_BROWSER_STYLE_H_

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): the popup of a Firefox
// add-on's Manifest V2 browser_action or page_action with "browser_style":
// true has Firefox's extension.css (resources/firefox_extension.css), which
// Firefox adds to such a popup's document as an author sheet. Firefox sizes
// the popup to its content as Chromium does, and that sheet's html and body
// have no margin: a body of 200 by 200 pixels makes a popup of that size,
// where the default 8-pixel margin made it 216 here. The sheet also gives the
// browser-style classes Firefox's look. Firefox ignores browser_style in
// Manifest V3, and other extensions' popups stay as Chromium shows them.

// Adds the sheet to `frame`, a new document at `document_url` of
// `extension`, if it is the top document of a Firefox add-on's popup whose
// action has browser_style. The add-on's page_action has its popup when the
// document is its default_popup; otherwise the browser_action has it.
void InsertFirefoxBrowserStyle(blink::WebLocalFrame* frame,
                               const Extension* extension,
                               const GURL& document_url);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_BROWSER_STYLE_H_
