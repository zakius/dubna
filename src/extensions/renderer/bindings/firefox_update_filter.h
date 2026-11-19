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
// `urls`, `properties`, `tabId`, `windowId` and `cookieStoreId`; and the
// RequestFilter of proxy.onRequest: `urls`, `types`, `tabId`, `windowId` and
// `incognito`, of which only `urls` and `incognito` filter: a request's type,
// tab and window aren't known where its proxy is decided. The listener runs
// only for an update or a request that matches each one that filters. Chromium's event filters are another thing, which these
// events don't support, so these are kept here, next to the event's
// listeners, and checked as it dispatches.
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
    // proxy.onRequest's.
    std::optional<bool> incognito;
  };

  FirefoxUpdateFilters();
  FirefoxUpdateFilters(const FirefoxUpdateFilters&) = delete;
  FirefoxUpdateFilters& operator=(const FirefoxUpdateFilters&) = delete;
  ~FirefoxUpdateFilters();

  // Whether the event takes Firefox's filter.
  static bool Takes(std::string_view event_name);

  // Reads `event_name`'s filter once, as Firefox's schema of it does.
  // nullopt for one Firefox refuses, with Firefox's message in `error`; or,
  // with no message, when a getter of the filter threw, whose exception is
  // then pending.
  static std::optional<Filter> Read(v8::Local<v8::Context> context,
                                    v8::Local<v8::Value> value,
                                    std::string_view event_name,
                                    std::string* error);

  void Add(v8::Isolate* isolate,
           v8::Local<v8::Function> listener,
           Filter filter);
  void Remove(v8::Local<v8::Function> listener);
  void Clear();

  // For each of `listeners`, the arguments it runs with for an update with
  // these arguments: the tab's id, the changes and the tab; or for a request
  // with proxy.onRequest's details. nullopt for a listener whose filter
  // doesn't match, which doesn't run. A listener without a filter gets
  // `args`. One whose filter has `properties` gets, as in Firefox, changes
  // with only the properties its filter names. Decided for all before any
  // runs, so that a listener another one removes keeps its filter for this
  // update. `current_window_id` is the window a `windowId` of
  // windows.WINDOW_ID_CURRENT names at this update, which the browser sends
  // with it (chrome/browser/extensions/api/tabs/firefox_current_window.h);
  // without one, such a filter matches no update, as in Firefox without a
  // window.
  std::vector<std::optional<v8::LocalVector<v8::Value>>> Arguments(
      const v8::LocalVector<v8::Function>& listeners,
      v8::Local<v8::Context> context,
      const v8::LocalVector<v8::Value>& args,
      std::optional<int> current_window_id) const;

 private:
  struct Entry {
    v8::Global<v8::Function> listener;
    Filter filter;
  };

  std::vector<Entry> entries_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_FIREFOX_UPDATE_FILTER_H_
