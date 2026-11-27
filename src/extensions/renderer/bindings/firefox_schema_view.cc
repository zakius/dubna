// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_schema_view.h"

#include "extensions/renderer/bindings/firefox_document_ids.h"

namespace extensions::firefox_schema_view {

namespace {

constexpr char kFirefoxOnly[] = "firefox_only";
constexpr char kChromium[] = "chromium";

AddOnCheck g_add_on_check = nullptr;
ResultFilter g_result_filter = nullptr;

}  // namespace

bool HasMarks(const base::Value& node) {
  if (node.is_dict()) {
    return HasMarks(node.GetDict());
  }
  if (node.is_list()) {
    for (const base::Value& item : node.GetList()) {
      if (HasMarks(item)) {
        return true;
      }
    }
  }
  return false;
}

bool HasMarks(const base::DictValue& node) {
  if (node.contains(kFirefoxOnly) || node.contains(kChromium)) {
    return true;
  }
  for (const auto [key, value] : node) {
    if (HasMarks(value)) {
      return true;
    }
  }
  return false;
}

bool IsFirefoxOnly(const base::Value& node) {
  return node.is_dict() &&
         node.GetDict().FindBool(kFirefoxOnly).value_or(false);
}

base::Value ChromiumView(const base::Value& node) {
  if (node.is_dict()) {
    return base::Value(ChromiumView(node.GetDict()));
  }
  if (!node.is_list()) {
    return node.Clone();
  }
  base::ListValue list;
  for (const base::Value& item : node.GetList()) {
    if (!IsFirefoxOnly(item)) {
      list.Append(ChromiumView(item));
    }
  }
  return base::Value(std::move(list));
}

base::DictValue ChromiumView(const base::DictValue& node) {
  base::DictValue dict;
  for (const auto [key, value] : node) {
    if (key != kChromium && key != kFirefoxOnly && !IsFirefoxOnly(value)) {
      dict.Set(key, ChromiumView(value));
    }
  }
  if (const base::DictValue* chromium = node.FindDict(kChromium)) {
    for (const auto [key, value] : *chromium) {
      if (value.is_none()) {
        dict.Remove(key);
      } else {
        dict.Set(key, value.Clone());
      }
    }
  }
  return dict;
}

void SetAddOnCheck(AddOnCheck check) {
  g_add_on_check = check;
}

bool IsFirefoxContext(v8::Local<v8::Context> context) {
  return g_add_on_check && g_add_on_check(context);
}

void SetResultFilter(ResultFilter filter) {
  g_result_filter = filter;
}

void FilterResults(v8::Local<v8::Context> context,
                   std::string_view name,
                   bool is_event,
                   const v8::LocalVector<v8::Value>& values) {
  if (g_result_filter) {
    g_result_filter(context, name, is_event, values);
  }
  GiveFirefoxDocumentIds(context, name, values);
}

}  // namespace extensions::firefox_schema_view
