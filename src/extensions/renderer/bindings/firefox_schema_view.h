// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_BINDINGS_FIREFOX_SCHEMA_VIEW_H_
#define EXTENSIONS_RENDERER_BINDINGS_FIREFOX_SCHEMA_VIEW_H_

#include <string_view>

#include "base/values.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

// Firefox add-ons (dubna's webextensions/README.md, "Firefox add-ons only"):
// what the climb adds to an API schema is marked, and only Firefox add-ons get
// it. A Firefox add-on's bindings read the schema as it is. Every other
// extension's read its Chromium view, which is Chromium's schema exactly:
// dubna's tools/firefox_gates.py checks that for every schema the patch
// changes. The marks:
//
// - "firefox_only": true leaves the node out of the Chromium view: a
//   function, event, property, type, parameter, choice, or an enum value
//   written {"name": "password", "firefox_only": true}.
// - "chromium": {...} gives the node's keys their Chromium values in the
//   Chromium view, and null leaves a key out: a property that only Firefox
//   add-ons may leave out has "optional": true, "chromium": {"optional": null}.
//
// The schema compiler ignores both keys, so the browser's generated types take
// what Firefox add-ons pass.
namespace extensions::firefox_schema_view {

// Whether `node` or a node in it is marked.
bool HasMarks(const base::Value& node);
bool HasMarks(const base::DictValue& node);

// `node` as Chromium has it.
base::Value ChromiumView(const base::Value& node);
base::DictValue ChromiumView(const base::DictValue& node);

// Whether `node` is left out of the Chromium view.
bool IsFirefoxOnly(const base::Value& node);

// Whether the bindings of `context` read the schemas as they are: a Firefox
// add-on's context. The bindings don't see extensions, so the bindings system
// sets the check (IsFirefoxAddOnContext); until then, no context is.
using AddOnCheck = bool (*)(v8::Local<v8::Context> context);
void SetAddOnCheck(AddOnCheck check);
bool IsFirefoxContext(v8::Local<v8::Context> context);

// Removes from `values`, the results of the function `name` ("tabs.query") or
// the arguments of the event `name` ("tabs.onUpdated") in `context`, the
// properties that the schema marks "firefox_only", such as a tab's
// cookieStoreId, unless `context` is a Firefox add-on's. The browser sends
// them to every extension. The bindings system sets the filter
// (RemoveFirefoxOnlyResults), which needs the schemas. A Firefox add-on gets
// the documentIds in `values` in Firefox's form (firefox_document_ids.h).
using ResultFilter = void (*)(v8::Local<v8::Context> context,
                              std::string_view name,
                              bool is_event,
                              const v8::LocalVector<v8::Value>& values);
void SetResultFilter(ResultFilter filter);
void FilterResults(v8::Local<v8::Context> context,
                   std::string_view name,
                   bool is_event,
                   const v8::LocalVector<v8::Value>& values);

}  // namespace extensions::firefox_schema_view

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_SCHEMA_VIEW_H_
