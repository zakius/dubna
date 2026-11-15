// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_
#define EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_

#include <string_view>
#include <vector>

#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the filter Firefox's
// tabs.onUpdated.addListener() takes after the listener, its extraParameters:
// `urls`, `properties`, `tabId`, `windowId` and `cookieStoreId`. The listener
// runs only for an update that matches each one given. Chromium's event
// filters are another thing, which tabs.onUpdated doesn't support, so these
// are kept here, next to the event's listeners, and checked as it dispatches.
class FirefoxUpdateFilters {
 public:
  FirefoxUpdateFilters();
  FirefoxUpdateFilters(const FirefoxUpdateFilters&) = delete;
  FirefoxUpdateFilters& operator=(const FirefoxUpdateFilters&) = delete;
  ~FirefoxUpdateFilters();

  // Whether the event takes Firefox's filter.
  static bool Takes(std::string_view event_name);

  void Add(v8::Isolate* isolate,
           v8::Local<v8::Function> listener,
           v8::Local<v8::Object> filter);
  void Remove(v8::Local<v8::Function> listener);
  void Clear();

  // Whether `listener` runs for an update with these arguments: the tab's id,
  // the changes and the tab. A listener without a filter always runs.
  bool Matches(v8::Local<v8::Function> listener,
               v8::Local<v8::Context> context,
               const v8::LocalVector<v8::Value>& args) const;

 private:
  struct Entry {
    v8::Global<v8::Function> listener;
    v8::Global<v8::Object> filter;
  };

  std::vector<Entry> entries_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_
