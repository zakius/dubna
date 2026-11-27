// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_BINDINGS_FIREFOX_DOCUMENT_IDS_H_
#define EXTENSIONS_RENDERER_BINDINGS_FIREFOX_DOCUMENT_IDS_H_

#include <string>
#include <string_view>

#include "base/values.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

// Firefox add-ons (dubna's webextensions/README.md): a documentId in Firefox's
// form. Firefox's is a UUID, 8-4-4-4-12 lowercase hex digits; Chromium's is
// the same 128 bits as 32 uppercase hex digits
// (ExtensionApiFrameIdMap::DocumentId::ToString()). The browser knows only
// Chromium's form, so the renderer gives a Firefox add-on's context
// (IsFirefoxContext()) Firefox's form and turns it back into Chromium's in
// what the add-on sends. An add-on may still send Chromium's form. Other
// extensions see Chromium's form only, and a UUID they send stays invalid.
namespace extensions {

// `id` in Firefox's form, if it is in Chromium's; else `id`.
std::string DocumentIdInFirefoxForm(std::string_view id);

// `id` in Chromium's form, if it is in Firefox's; else `id`.
std::string DocumentIdInChromiumForm(std::string_view id);

// The documentId `id`, from the browser, as `context` gets it: in Firefox's
// form in a Firefox add-on's, such as a runtime.MessageSender's.
std::string GiveFirefoxDocumentId(v8::Local<v8::Context> context,
                                  std::string_view id);

// The documentId `id`, from `context`, as the browser takes it: in Chromium's
// form, if a Firefox add-on gave it in Firefox's, such as tabs.sendMessage's
// option.
std::string TakeFirefoxDocumentId(v8::Local<v8::Context> context,
                                  std::string_view id);

// Gives Firefox's form to the documentIds in `values`, the results of the
// function `name` ("webNavigation.getFrame") or the arguments of the event
// `name` in `context`, if a Firefox add-on's: webNavigation's, webRequest's,
// proxy.onRequest's, and the results of scripting.executeScript,
// userScripts.execute and runtime.getContexts. firefox_schema_view.h's
// FilterResults() calls it.
void GiveFirefoxDocumentIds(v8::Local<v8::Context> context,
                            std::string_view name,
                            const v8::LocalVector<v8::Value>& values);

// Turns the documentIds in `arguments`, of the function `method` that
// `context` calls, into Chromium's form, if a Firefox add-on gave them in
// Firefox's: webNavigation.getFrame's, the target of scripting's and
// userScripts.execute's, runtime.getContexts's filter, and the Chromium APIs
// that take one.
void TakeFirefoxDocumentIds(v8::Local<v8::Context> context,
                            std::string_view method,
                            base::ListValue& arguments);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_DOCUMENT_IDS_H_
