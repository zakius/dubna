// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_update_filter.h"

#include <algorithm>
#include <utility>

#include "base/strings/strcat.h"
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

// Firefox's UpdatePropertyName (browser/components/extensions/schemas/
// tabs.json).
constexpr std::string_view kPropertyNames[] = {
    "attention",    "audible",     "autoDiscardable", "discarded", "favIconUrl",
    "groupId",      "hidden",      "isArticle",       "mutedInfo", "pinned",
    "sharingState", "splitViewId", "status",          "title",     "url"};

// `object[key]`, or an empty handle when it is missing, null or undefined.
v8::Local<v8::Value> Get(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object,
                         std::string_view key) {
  v8::Local<v8::Value> value;
  if (!object->Get(context, gin::StringToSymbol(v8::Isolate::GetCurrent(), key))
           .ToLocal(&value) ||
      value->IsNullOrUndefined()) {
    return v8::Local<v8::Value>();
  }
  return value;
}

std::string TypeError(std::string_view detail) {
  return base::StrCat({"Type error for parameter filter (", detail,
                       ") for tabs.onUpdated.addListener."});
}

// `filter[key]` as a non-empty array of strings.
std::optional<std::vector<std::string>> ReadStrings(std::string_view key,
                                                    v8::Local<v8::Value> value,
                                                    std::string* error) {
  std::vector<std::string> strings;
  if (!value->IsArray() ||
      !gin::ConvertFromV8(v8::Isolate::GetCurrent(), value, &strings)) {
    *error = TypeError(base::StrCat(
        {"Error processing ", key, ": Expected an array of strings"}));
    return std::nullopt;
  }
  if (strings.empty()) {
    *error = TypeError(base::StrCat(
        {"Error processing ", key, ": Array requires at least 1 items"}));
    return std::nullopt;
  }
  return strings;
}

bool ReadInteger(v8::Local<v8::Value> value,
                 std::string_view key,
                 std::optional<int>* out,
                 std::string* error) {
  if (value.IsEmpty()) {
    return true;
  }
  if (!value->IsInt32()) {
    *error = TypeError(
        base::StrCat({"Error processing ", key, ": Expected integer"}));
    return false;
  }
  *out = value.As<v8::Int32>()->Value();
  return true;
}

// An update as the filters see it: what they read of tabs.onUpdated's
// arguments (tabId, changeInfo, tab), taken once from the objects' own data
// properties, so that no add-on script runs while the filters are checked.
struct Update {
  std::optional<int> tab_id;
  std::optional<int> window_id;
  std::optional<std::string> cookie_store_id;
  GURL url;
  std::vector<std::string> changed;
};

// `object`'s own property `key`, or an empty handle.
v8::Local<v8::Value> Own(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object,
                         std::string_view key) {
  v8::Local<v8::String> name =
      gin::StringToSymbol(v8::Isolate::GetCurrent(), key);
  v8::Local<v8::Value> value;
  if (!object->HasOwnProperty(context, name).FromMaybe(false) ||
      !object->Get(context, name).ToLocal(&value)) {
    return v8::Local<v8::Value>();
  }
  return value;
}

std::optional<int> OwnInteger(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> object,
                              std::string_view key) {
  v8::Local<v8::Value> value = Own(context, object, key);
  if (value.IsEmpty() || !value->IsInt32()) {
    return std::nullopt;
  }
  return value.As<v8::Int32>()->Value();
}

std::optional<std::string> OwnString(v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> object,
                                     std::string_view key) {
  v8::Local<v8::Value> value = Own(context, object, key);
  std::string text;
  if (value.IsEmpty() || !value->IsString() ||
      !gin::ConvertFromV8(v8::Isolate::GetCurrent(), value, &text)) {
    return std::nullopt;
  }
  return text;
}

// nullopt for arguments of another shape, for which every listener runs.
std::optional<Update> ReadUpdate(v8::Local<v8::Context> context,
                                 const v8::LocalVector<v8::Value>& args) {
  if (args.size() < 3 || !args[1]->IsObject() || !args[2]->IsObject()) {
    return std::nullopt;
  }
  v8::Local<v8::Object> changes = args[1].As<v8::Object>();
  v8::Local<v8::Object> tab = args[2].As<v8::Object>();
  Update update;
  if (args[0]->IsInt32()) {
    update.tab_id = args[0].As<v8::Int32>()->Value();
  }
  update.window_id = OwnInteger(context, tab, "windowId");
  update.cookie_store_id = OwnString(context, tab, "cookieStoreId");
  // A tab whose URL the add-on may not see has none, and matches no URL.
  update.url = GURL(OwnString(context, tab, "url").value_or(std::string()));
  v8::Local<v8::Array> names;
  if (changes->GetOwnPropertyNames(context).ToLocal(&names)) {
    gin::ConvertFromV8(v8::Isolate::GetCurrent(), names, &update.changed);
  }
  return update;
}

bool Matches(const FirefoxUpdateFilters::Filter& filter, const Update& update) {
  if ((filter.tab_id && update.tab_id != filter.tab_id) ||
      (filter.window_id && update.window_id != filter.window_id) ||
      (filter.cookie_store_id &&
       update.cookie_store_id != filter.cookie_store_id)) {
    return false;
  }
  // As in Firefox, a listener for some properties runs when one of them
  // changed. Firefox's names that Chromium's changes don't have (`hidden`,
  // `isArticle`, `attention`, `sharingState`) never match.
  if (filter.properties &&
      std::ranges::none_of(*filter.properties, [&](const std::string& name) {
        return std::ranges::find(update.changed, name) != update.changed.end();
      })) {
    return false;
  }
  if (filter.urls &&
      (!update.url.is_valid() ||
       std::ranges::none_of(*filter.urls, [&](const URLPattern& pattern) {
         return pattern.MatchesURL(update.url);
       }))) {
    return false;
  }
  return true;
}

}  // namespace

FirefoxUpdateFilters::Filter::Filter() = default;
FirefoxUpdateFilters::Filter::Filter(Filter&&) = default;
FirefoxUpdateFilters::Filter& FirefoxUpdateFilters::Filter::operator=(
    Filter&&) = default;
FirefoxUpdateFilters::Filter::~Filter() = default;

FirefoxUpdateFilters::FirefoxUpdateFilters() = default;
FirefoxUpdateFilters::~FirefoxUpdateFilters() = default;

// static
bool FirefoxUpdateFilters::Takes(std::string_view event_name) {
  return event_name == "tabs.onUpdated";
}

// static
std::optional<FirefoxUpdateFilters::Filter> FirefoxUpdateFilters::Read(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> value,
    std::string* error) {
  Filter filter;
  if (v8::Local<v8::Value> urls = Get(context, value, "urls");
      !urls.IsEmpty()) {
    std::optional<std::vector<std::string>> patterns =
        ReadStrings("urls", urls, error);
    if (!patterns) {
      return std::nullopt;
    }
    filter.urls.emplace();
    for (const std::string& text : *patterns) {
      URLPattern pattern(URLPattern::SCHEME_ALL);
      if (pattern.Parse(text) != URLPattern::ParseResult::kSuccess) {
        *error = base::StrCat({"Invalid url pattern: ", text});
        return std::nullopt;
      }
      filter.urls->push_back(std::move(pattern));
    }
  }
  if (v8::Local<v8::Value> properties = Get(context, value, "properties");
      !properties.IsEmpty()) {
    filter.properties = ReadStrings("properties", properties, error);
    if (!filter.properties) {
      return std::nullopt;
    }
    for (const std::string& name : *filter.properties) {
      if (std::ranges::find(kPropertyNames, name) == std::end(kPropertyNames)) {
        *error = TypeError(base::StrCat(
            {"Error processing properties: Invalid enumeration value \"", name,
             "\""}));
        return std::nullopt;
      }
    }
  }
  if (!ReadInteger(Get(context, value, "tabId"), "tabId", &filter.tab_id,
                   error) ||
      !ReadInteger(Get(context, value, "windowId"), "windowId",
                   &filter.window_id, error)) {
    return std::nullopt;
  }
  if (v8::Local<v8::Value> store = Get(context, value, "cookieStoreId");
      !store.IsEmpty()) {
    if (!store->IsString()) {
      *error = TypeError("Error processing cookieStoreId: Expected string");
      return std::nullopt;
    }
    filter.cookie_store_id = std::string();
    gin::ConvertFromV8(v8::Isolate::GetCurrent(), store,
                       &*filter.cookie_store_id);
  }
  return filter;
}

void FirefoxUpdateFilters::Add(v8::Isolate* isolate,
                               v8::Local<v8::Function> listener,
                               Filter filter) {
  entries_.push_back(
      {v8::Global<v8::Function>(isolate, listener), std::move(filter)});
}

void FirefoxUpdateFilters::Remove(v8::Local<v8::Function> listener) {
  std::erase_if(entries_, [&listener](const Entry& entry) {
    return entry.listener == listener;
  });
}

void FirefoxUpdateFilters::Clear() {
  entries_.clear();
}

std::vector<bool> FirefoxUpdateFilters::Matching(
    const v8::LocalVector<v8::Function>& listeners,
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& args) const {
  std::vector<bool> runs(listeners.size(), true);
  std::optional<Update> update = ReadUpdate(context, args);
  if (!update) {
    return runs;
  }
  for (size_t i = 0; i < listeners.size(); ++i) {
    for (const Entry& entry : entries_) {
      if (entry.listener == listeners[i]) {
        runs[i] = Matches(entry.filter, *update);
      }
    }
  }
  return runs;
}

}  // namespace extensions
