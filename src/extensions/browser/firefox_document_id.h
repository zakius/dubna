// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_DOCUMENT_ID_H_
#define EXTENSIONS_BROWSER_FIREFOX_DOCUMENT_ID_H_

#include <string>

#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): runtime.getDocumentId
// (extensions/renderer/api/firefox_document_id.h). The documentId, as
// webNavigation, webRequest and scripting give it, of the current document of
// the frame that `frame_token` names in `caller`'s process: a frame there, or
// the placeholder of a frame in another process, such as a cross-site frame
// or parent. Empty when there is no such frame.
std::string FirefoxDocumentId(content::RenderFrameHost& caller,
                              const blink::FrameToken& frame_token);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FIREFOX_DOCUMENT_ID_H_
