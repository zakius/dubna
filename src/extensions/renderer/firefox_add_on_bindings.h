// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_ADD_ON_BINDINGS_H_
#define EXTENSIONS_RENDERER_FIREFOX_ADD_ON_BINDINGS_H_

#include <string_view>

#include "base/values.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md, "Firefox add-ons only"):
// whether `context` is a Firefox add-on's (IsFirefoxAddOn()). The bindings
// read their schemas' Chromium view for any other context
// (firefox_schema_view.h), which this sets as their check.
bool IsFirefoxAddOnContext(v8::Local<v8::Context> context);

// Leaves out of `object`, an API's binding in `context` from the API's
// `schema`, the members that the schema marks "firefox_only", unless
// `context` is a Firefox add-on's: functions, events, properties at any
// depth, enum types and enum values. A Chrome Web Store extension that tells
// Chrome from Firefox by `'onShown' in chrome.contextMenus` then takes its
// Chrome path.
void HideFirefoxOnlyMembers(v8::Local<v8::Context> context,
                            const base::DictValue& schema,
                            v8::Local<v8::Object> object);

// firefox_schema_view.h's ResultFilter, which the bindings system sets.
void RemoveFirefoxOnlyResults(v8::Local<v8::Context> context,
                              std::string_view name,
                              bool is_event,
                              const v8::LocalVector<v8::Value>& values);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_ADD_ON_BINDINGS_H_
