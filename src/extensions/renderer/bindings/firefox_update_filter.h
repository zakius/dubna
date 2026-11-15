// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_
#define EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "extensions/common/url_pattern.h"
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
  // A filter as addListener() read it. A property that is missing or null is
  // no filter, as in Firefox.
  struct Filter {
    Filter();
    Filter(Filter&&);
    Filter& operator=(Filter&&);
    ~Filter();

    std::optional<std::vector<URLPattern>> urls;
    std::optional<std::vector<std::string>> properties;
    std::optional<int> tab_id;
    std::optional<int> window_id;
    std::optional<std::string> cookie_store_id;
  };

  FirefoxUpdateFilters();
  FirefoxUpdateFilters(const FirefoxUpdateFilters&) = delete;
  FirefoxUpdateFilters& operator=(const FirefoxUpdateFilters&) = delete;
  ~FirefoxUpdateFilters();

  // Whether the event takes Firefox's filter.
  static bool Takes(std::string_view event_name);

  // Reads `value`, the filter object, once, as Firefox's schema of it does.
  // nullopt, with Firefox's message in `error`, for one Firefox refuses.
  static std::optional<Filter> Read(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> value,
                                    std::string* error);

  void Add(v8::Isolate* isolate,
           v8::Local<v8::Function> listener,
           Filter filter);
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
    Filter filter;
  };

  std::vector<Entry> entries_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_
