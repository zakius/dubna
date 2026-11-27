// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_API_FIREFOX_DOCUMENT_ID_H_
#define EXTENSIONS_RENDERER_API_FIREFOX_DOCUMENT_ID_H_

#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

class APISignature;
class APITypeReferenceMap;
class ScriptContext;

// Firefox add-ons (dubna's webextensions/README.md): runtime.getDocumentId,
// which RuntimeHooksDelegate hands here. It returns the documentId that
// webNavigation, webRequest and scripting give for the document of a window,
// local or remote, or of an <iframe>, <frame>, <embed> or <object> element,
// synchronously and in Firefox's form (firefox_document_ids.h) as in
// Firefox. The ID is the browser's, so the renderer asks for it with a
// synchronous message (extensions/browser/firefox_document_id.h). Other
// targets, and frames that are gone, throw Firefox's errors.
APIBindingHooks::RequestResult HandleFirefoxGetDocumentId(
    ScriptContext* script_context,
    const APISignature* signature,
    const v8::LocalVector<v8::Value>& arguments,
    const APITypeReferenceMap& refs);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_FIREFOX_DOCUMENT_ID_H_
