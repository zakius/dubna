// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_update_filter.h"

#include <algorithm>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

// Firefox's UpdateFilter keys.
constexpr std::string_view kFilterKeys[] = {"urls", "properties", "tabId",
                                            "windowId", "cookieStoreId"};

// Reads a filter object as Firefox's schema does, with Firefox's messages.
// A getter of the object can throw: then the exception is pending, and the
// read stops with no message.
class FilterReader {
 public:
  FilterReader(v8::Local<v8::Context> context,
               v8::Local<v8::Object> object,
               std::string* error)
      : context_(context), object_(object), error_(error) {}

  bool failed() const { return failed_; }

  // Refuses the keys Firefox's schema doesn't have.
  void CheckKeys() {
    v8::Local<v8::Array> names;
    std::vector<std::string> keys;
    if (!object_->GetOwnPropertyNames(context_).ToLocal(&names)) {
      failed_ = true;
      return;
    }
    gin::ConvertFromV8(v8::Isolate::GetCurrent(), names, &keys);
    for (const std::string& key : keys) {
      if (std::ranges::find(kFilterKeys, key) == std::end(kFilterKeys)) {
        Fail(base::StrCat({"Unexpected property \"", key, "\""}));
        return;
      }
    }
  }

  // `object[key]`, or an empty handle when it is missing, null or undefined.
  v8::Local<v8::Value> Get(std::string_view key) {
    v8::Local<v8::Value> value;
    if (failed_) {
      return value;
    }
    if (!object_
             ->Get(context_,
                   gin::StringToSymbol(v8::Isolate::GetCurrent(), key))
             .ToLocal(&value)) {
      failed_ = true;
      return v8::Local<v8::Value>();
    }
    return value->IsNullOrUndefined() ? v8::Local<v8::Value>() : value;
  }

  // `object[key]` as a non-empty array of strings.
  std::optional<std::vector<std::string>> Strings(std::string_view key) {
    v8::Local<v8::Value> value = Get(key);
    if (value.IsEmpty()) {
      return std::nullopt;
    }
    if (!value->IsArray()) {
      Fail(base::StrCat({"Error processing ", key, ": Expected array"}));
      return std::nullopt;
    }
    v8::Local<v8::Array> array = value.As<v8::Array>();
    if (array->Length() == 0) {
      Fail(base::StrCat({"Error processing ", key,
                         ": Array requires at least 1 items; you have 0"}));
      return std::nullopt;
    }
    std::vector<std::string> strings;
    for (uint32_t i = 0; i < array->Length(); ++i) {
      v8::Local<v8::Value> item;
      if (!array->Get(context_, i).ToLocal(&item)) {
        failed_ = true;
        return std::nullopt;
      }
      std::string text;
      if (!item->IsString() ||
          !gin::ConvertFromV8(v8::Isolate::GetCurrent(), item, &text)) {
        Fail(base::StrCat({"Error processing ", key, ".",
                           base::NumberToString(i), ": Expected string"}));
        return std::nullopt;
      }
      strings.push_back(std::move(text));
    }
    return strings;
  }

  std::optional<int> Integer(std::string_view key) {
    v8::Local<v8::Value> value = Get(key);
    if (value.IsEmpty()) {
      return std::nullopt;
    }
    if (!value->IsInt32()) {
      Fail(base::StrCat({"Error processing ", key, ": Expected integer"}));
      return std::nullopt;
    }
    return value.As<v8::Int32>()->Value();
  }

  std::optional<std::string> String(std::string_view key) {
    v8::Local<v8::Value> value = Get(key);
    std::string text;
    if (value.IsEmpty()) {
      return std::nullopt;
    }
    if (!value->IsString() ||
        !gin::ConvertFromV8(v8::Isolate::GetCurrent(), value, &text)) {
      Fail(base::StrCat({"Error processing ", key, ": Expected string"}));
      return std::nullopt;
    }
    return text;
  }

  void Fail(std::string_view detail) {
    if (!failed_) {
      *error_ = base::StrCat({"Type error for parameter filter (", detail,
                              ") for tabs.onUpdated."});
      failed_ = true;
    }
  }

 private:
  v8::Local<v8::Context> context_;
  v8::Local<v8::Object> object_;
  raw_ptr<std::string> error_;
  bool failed_ = false;
};

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
    v8::Local<v8::Value> value,
    std::string* error) {
  if (!value->IsObject() || value->IsArray() || value->IsFunction()) {
    *error = "Incorrect argument types for tabs.onUpdated.";
    return std::nullopt;
  }
  FilterReader reader(context, value.As<v8::Object>(), error);
  reader.CheckKeys();
  Filter filter;
  if (std::optional<std::vector<std::string>> patterns =
          reader.Strings("urls")) {
    filter.urls.emplace();
    for (const std::string& text : *patterns) {
      // Firefox builds the patterns after addListener() has returned, so one
      // that doesn't parse throws nothing there: it matches no URL. So do
      // Firefox's own schemes here, such as moz-extension:.
      URLPattern pattern(URLPattern::SCHEME_ALL);
      if (pattern.Parse(text) == URLPattern::ParseResult::kSuccess) {
        filter.urls->push_back(std::move(pattern));
      }
    }
  }
  filter.properties = reader.Strings("properties");
  if (filter.properties) {
    for (size_t i = 0; i < filter.properties->size(); ++i) {
      const std::string& name = (*filter.properties)[i];
      if (std::ranges::find(kPropertyNames, name) == std::end(kPropertyNames)) {
        reader.Fail(base::StrCat(
            {"Error processing properties.", base::NumberToString(i),
             ": Invalid enumeration value \"", name, "\""}));
      }
    }
  }
  filter.tab_id = reader.Integer("tabId");
  filter.window_id = reader.Integer("windowId");
  filter.cookie_store_id = reader.String("cookieStoreId");
  if (reader.failed()) {
    return std::nullopt;
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
