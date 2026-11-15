// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_update_filter.h"

#include <string>

#include "extensions/common/url_pattern.h"
#include "gin/converter.h"
#include "url/gurl.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

// `object[key]`, or an empty handle.
v8::Local<v8::Value> Get(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object,
                         const char* key) {
  v8::Local<v8::Value> value;
  if (!object->Get(context, gin::StringToSymbol(v8::Isolate::GetCurrent(), key))
           .ToLocal(&value) ||
      value->IsUndefined()) {
    return v8::Local<v8::Value>();
  }
  return value;
}

// Whether `object[key]` is `wanted`, or `wanted` is empty: no filter.
bool SameValue(v8::Local<v8::Context> context,
               v8::Local<v8::Object> object,
               const char* key,
               v8::Local<v8::Value> wanted) {
  if (wanted.IsEmpty()) {
    return true;
  }
  v8::Local<v8::Value> value = Get(context, object, key);
  return !value.IsEmpty() && value->StrictEquals(wanted);
}

// Whether one of the names in `properties` is a key of `changes`: as in
// Firefox, a listener for some properties runs when one of them changed.
// Firefox's names that Chromium's changes don't have (`hidden`, `isArticle`,
// `attention`, `sharingState`) never match.
bool AnyChanged(v8::Local<v8::Context> context,
                v8::Local<v8::Array> properties,
                v8::Local<v8::Object> changes) {
  for (uint32_t i = 0; i < properties->Length(); ++i) {
    v8::Local<v8::Value> name;
    if (!properties->Get(context, i).ToLocal(&name) || !name->IsString()) {
      continue;
    }
    v8::Maybe<bool> has =
        changes->HasOwnProperty(context, name.As<v8::String>());
    if (has.IsJust() && has.FromJust()) {
      return true;
    }
  }
  return false;
}

// Whether the tab's URL matches one of the match patterns in `urls`. A tab
// whose URL the add-on may not see has none, and matches none.
bool AnyUrlMatches(v8::Local<v8::Context> context,
                   v8::Local<v8::Array> urls,
                   v8::Local<v8::Object> tab) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> tab_url = Get(context, tab, "url");
  std::string spec;
  if (tab_url.IsEmpty() || !gin::ConvertFromV8(isolate, tab_url, &spec)) {
    return false;
  }
  GURL url(spec);
  if (!url.is_valid()) {
    return false;
  }
  for (uint32_t i = 0; i < urls->Length(); ++i) {
    v8::Local<v8::Value> value;
    std::string pattern_text;
    if (!urls->Get(context, i).ToLocal(&value) ||
        !gin::ConvertFromV8(isolate, value, &pattern_text)) {
      continue;
    }
    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(pattern_text) == URLPattern::ParseResult::kSuccess &&
        pattern.MatchesURL(url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

FirefoxUpdateFilters::FirefoxUpdateFilters() = default;
FirefoxUpdateFilters::~FirefoxUpdateFilters() = default;

// static
bool FirefoxUpdateFilters::Takes(std::string_view event_name) {
  return event_name == "tabs.onUpdated";
}

void FirefoxUpdateFilters::Add(v8::Isolate* isolate,
                               v8::Local<v8::Function> listener,
                               v8::Local<v8::Object> filter) {
  entries_.push_back({v8::Global<v8::Function>(isolate, listener),
                      v8::Global<v8::Object>(isolate, filter)});
}

void FirefoxUpdateFilters::Remove(v8::Local<v8::Function> listener) {
  std::erase_if(entries_, [&listener](const Entry& entry) {
    return entry.listener == listener;
  });
}

void FirefoxUpdateFilters::Clear() {
  entries_.clear();
}

bool FirefoxUpdateFilters::Matches(
    v8::Local<v8::Function> listener,
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& args) const {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Object> filter;
  for (const Entry& entry : entries_) {
    if (entry.listener == listener) {
      filter = entry.filter.Get(isolate);
    }
  }
  // tabs.onUpdated's arguments are (tabId, changeInfo, tab).
  if (filter.IsEmpty() || args.size() < 3 || !args[1]->IsObject() ||
      !args[2]->IsObject()) {
    return true;
  }
  v8::Local<v8::Object> changes = args[1].As<v8::Object>();
  v8::Local<v8::Object> tab = args[2].As<v8::Object>();

  v8::Local<v8::Value> tab_id = Get(context, filter, "tabId");
  if (!tab_id.IsEmpty() && !args[0]->StrictEquals(tab_id)) {
    return false;
  }
  if (!SameValue(context, tab, "windowId", Get(context, filter, "windowId")) ||
      !SameValue(context, tab, "cookieStoreId",
                 Get(context, filter, "cookieStoreId"))) {
    return false;
  }
  v8::Local<v8::Value> properties = Get(context, filter, "properties");
  if (!properties.IsEmpty() && properties->IsArray() &&
      !AnyChanged(context, properties.As<v8::Array>(), changes)) {
    return false;
  }
  v8::Local<v8::Value> urls = Get(context, filter, "urls");
  if (!urls.IsEmpty() && urls->IsArray() &&
      !AnyUrlMatches(context, urls.As<v8::Array>(), tab)) {
    return false;
  }
  return true;
}

}  // namespace extensions
