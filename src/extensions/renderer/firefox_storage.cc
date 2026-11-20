// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_storage.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

bool InFirefoxAddOn(const ScriptContext& script_context) {
  const Extension* extension = script_context.extension();
  return extension && IsFirefoxAddOn(*extension);
}

// Puts `value` in `items` as `key`, unless the area gave `key`.
void AddDefault(v8::Local<v8::Context> context,
                v8::Local<v8::Object> items,
                const std::string& key,
                v8::Local<v8::Value> value) {
  v8::Local<v8::String> name =
      gin::StringToV8(v8::Isolate::GetCurrent(), key);
  bool has_key = false;
  if (!items->HasOwnProperty(context, name).To(&has_key) || has_key) {
    return;
  }
  std::ignore = items->CreateDataProperty(context, name, value);
}

binding::V8ArgumentList AddDefaults(base::DictValue defaults,
                                    std::vector<std::string> undefined_keys,
                                    const binding::V8ArgumentList& result,
                                    v8::Local<v8::Context> context,
                                    binding::AsyncResponseType) {
  if (result.empty() || !result[0]->IsObject()) {
    return result;
  }
  v8::Local<v8::Object> items = result[0].As<v8::Object>();
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  for (const auto [key, value] : defaults) {
    AddDefault(context, items, key, converter->ToV8Value(value, context));
  }
  for (const std::string& key : undefined_keys) {
    AddDefault(context, items, key, v8::Undefined(v8::Isolate::GetCurrent()));
  }
  return result;
}

}  // namespace

bool IsHiddenFromFirefoxAddOn(const ScriptContext& script_context,
                              std::string_view feature_name) {
  return feature_name == "storage.session" &&
         script_context.context_type() == mojom::ContextType::kContentScript &&
         InFirefoxAddOn(script_context);
}

binding::ResultModifierFunction FirefoxStorageGetDefaults(
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& arguments,
    base::Value& keys) {
  // Only the object form parses to a dictionary, and it is get()'s first
  // argument.
  if (!keys.is_dict() || arguments.empty() || !arguments[0]->IsObject()) {
    return binding::ResultModifierFunction();
  }
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  if (!script_context || !InFirefoxAddOn(*script_context)) {
    return binding::ResultModifierFunction();
  }

  // The keys the bindings dropped are the object's own enumerable keys, the
  // list the bindings read, that the parsed object lacks. Should listing them
  // throw this time (a proxy), the call goes on as Chromium's.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Array> names;
  if (!arguments[0]
           .As<v8::Object>()
           ->GetOwnPropertyNames(context)
           .ToLocal(&names)) {
    return binding::ResultModifierFunction();
  }
  base::DictValue& defaults = keys.GetDict();
  std::vector<std::string> undefined_keys;
  for (uint32_t i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Value> name;
    if (!names->Get(context, i).ToLocal(&name)) {
      return binding::ResultModifierFunction();
    }
    v8::String::Utf8Value utf8_name(isolate, name);
    std::string key(*utf8_name, utf8_name.length());
    if (!defaults.contains(key)) {
      undefined_keys.push_back(std::move(key));
    }
  }
  if (undefined_keys.empty()) {
    return binding::ResultModifierFunction();
  }

  base::ListValue all_keys;
  for (const auto [key, value] : defaults) {
    all_keys.Append(key);
  }
  for (const std::string& key : undefined_keys) {
    all_keys.Append(key);
  }
  base::DictValue taken_defaults = std::move(defaults);
  keys = base::Value(std::move(all_keys));
  return base::BindOnce(&AddDefaults, std::move(taken_defaults),
                        std::move(undefined_keys));
}

}  // namespace extensions
