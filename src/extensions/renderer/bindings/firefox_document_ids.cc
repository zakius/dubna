// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_document_ids.h"

#include <tuple>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "extensions/renderer/bindings/firefox_schema_view.h"
#include "gin/converter.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

// Where Firefox's form has its dashes: 8-4-4-4-12 hex digits.
constexpr size_t kDashes[] = {8, 13, 18, 23};

// The functions and events whose results or arguments carry documentIds, in
// objects or in arrays of objects. A namespace ends with a dot. webRequest's
// events reach the bindings by names such as "webRequest.onCompleted/g3".
constexpr std::string_view kGiven[] = {
    "webNavigation.",          "webRequest.",         "proxy.onRequest",
    "scripting.executeScript", "userScripts.execute", "runtime.getContexts",
};

// The functions that take documentIds in their first argument: as
// "documentId", "documentIds", or "documentIds" of its "target".
constexpr std::string_view kTaken[] = {
    "webNavigation.getFrame",
    "scripting.executeScript",
    "scripting.insertCSS",
    "scripting.removeCSS",
    "userScripts.execute",
    "runtime.getContexts",
    "cookies.getPartitionKey",
    "permissions.addSiteAccessRequest",
    "permissions.removeSiteAccessRequest",
};

bool IsChromiumForm(std::string_view id) {
  if (id.size() != 32) {
    return false;
  }
  for (char c : id) {
    if (!base::IsAsciiDigit(c) && !(c >= 'A' && c <= 'F')) {
      return false;
    }
  }
  return true;
}

bool IsFirefoxForm(std::string_view id) {
  if (id.size() != 36) {
    return false;
  }
  for (size_t i = 0; i < id.size(); ++i) {
    if (base::Contains(kDashes, i)) {
      if (id[i] != '-') {
        return false;
      }
    } else if (!base::IsHexDigit(id[i])) {
      return false;
    }
  }
  return true;
}

bool IsGiven(std::string_view name) {
  for (std::string_view given : kGiven) {
    if (given.ends_with('.') ? name.starts_with(given) : name == given) {
      return true;
    }
  }
  return false;
}

// Gives Firefox's form to the documentIds that `object` has.
void GiveToObject(v8::Local<v8::Context> context,
                  v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  for (const char* key : {"documentId", "parentDocumentId"}) {
    v8::Local<v8::String> v8_key = gin::StringToSymbol(isolate, key);
    v8::Local<v8::Value> value;
    if (!object->HasOwnProperty(context, v8_key).FromMaybe(false) ||
        !object->Get(context, v8_key).ToLocal(&value) || !value->IsString()) {
      continue;
    }
    std::string id = gin::V8ToString(isolate, value);
    if (IsChromiumForm(id)) {
      std::ignore = object->CreateDataProperty(
          context, v8_key,
          gin::StringToV8(isolate, DocumentIdInFirefoxForm(id)));
    }
  }
}

void TakeFromList(base::ListValue* list) {
  if (!list) {
    return;
  }
  for (base::Value& id : *list) {
    if (id.is_string()) {
      id = base::Value(DocumentIdInChromiumForm(id.GetString()));
    }
  }
}

}  // namespace

std::string DocumentIdInFirefoxForm(std::string_view id) {
  if (!IsChromiumForm(id)) {
    return std::string(id);
  }
  std::string hex = base::ToLowerASCII(id);
  std::string_view digits(hex);
  return base::StrCat({digits.substr(0, 8), "-", digits.substr(8, 4), "-",
                       digits.substr(12, 4), "-", digits.substr(16, 4), "-",
                       digits.substr(20)});
}

std::string DocumentIdInChromiumForm(std::string_view id) {
  if (!IsFirefoxForm(id)) {
    return std::string(id);
  }
  std::string hex;
  for (char c : id) {
    if (c != '-') {
      hex += base::ToUpperASCII(c);
    }
  }
  return hex;
}

std::string GiveFirefoxDocumentId(v8::Local<v8::Context> context,
                                  std::string_view id) {
  return firefox_schema_view::IsFirefoxContext(context)
             ? DocumentIdInFirefoxForm(id)
             : std::string(id);
}

std::string TakeFirefoxDocumentId(v8::Local<v8::Context> context,
                                  std::string_view id) {
  return firefox_schema_view::IsFirefoxContext(context)
             ? DocumentIdInChromiumForm(id)
             : std::string(id);
}

void GiveFirefoxDocumentIds(v8::Local<v8::Context> context,
                            std::string_view name,
                            const v8::LocalVector<v8::Value>& values) {
  if (!IsGiven(name) || !firefox_schema_view::IsFirefoxContext(context)) {
    return;
  }
  for (v8::Local<v8::Value> value : values) {
    if (value.IsEmpty() || !value->IsObject()) {
      continue;
    }
    if (!value->IsArray()) {
      GiveToObject(context, value.As<v8::Object>());
      continue;
    }
    v8::Local<v8::Array> array = value.As<v8::Array>();
    for (uint32_t i = 0; i < array->Length(); ++i) {
      v8::Local<v8::Value> item;
      if (array->Get(context, i).ToLocal(&item) && item->IsObject() &&
          !item->IsArray()) {
        GiveToObject(context, item.As<v8::Object>());
      }
    }
  }
}

void TakeFirefoxDocumentIds(v8::Local<v8::Context> context,
                            std::string_view method,
                            base::ListValue& arguments) {
  if (!base::Contains(kTaken, method) || arguments.empty() ||
      !arguments[0].is_dict() ||
      !firefox_schema_view::IsFirefoxContext(context)) {
    return;
  }
  base::DictValue& details = arguments[0].GetDict();
  if (const std::string* id = details.FindString("documentId")) {
    details.Set("documentId", DocumentIdInChromiumForm(*id));
  }
  TakeFromList(details.FindList("documentIds"));
  if (base::DictValue* target = details.FindDict("target")) {
    TakeFromList(target->FindList("documentIds"));
  }
}

}  // namespace extensions
