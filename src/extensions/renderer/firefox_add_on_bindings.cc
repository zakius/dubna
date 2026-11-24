// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_add_on_bindings.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/renderer/bindings/firefox_schema_view.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"

namespace extensions {

namespace {

using firefox_schema_view::HasMarks;
using firefox_schema_view::IsFirefoxOnly;

// An enum value's name on its enum object, as api_binding.cc's
// GetJSEnumEntryName() makes it: "tools_menu" is TOOLS_MENU.
std::string EnumEntryName(std::string_view value) {
  std::string result;
  if (!value.empty() && base::IsAsciiDigit(value[0])) {
    result.push_back('_');
  }
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0 && base::IsAsciiLower(value[i - 1]) &&
        base::IsAsciiUpper(value[i])) {
      result.push_back('_');
      result.push_back(value[i]);
    } else if (value[i] == '-') {
      result.push_back('_');
    } else {
      result.push_back(base::ToUpperASCII(value[i]));
    }
  }
  return result;
}

void Delete(v8::Local<v8::Context> context,
            v8::Local<v8::Object> object,
            std::string_view name) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  std::ignore = object->Delete(context, gin::StringToSymbol(isolate, name));
}

// The object at `name` in `object`, or an empty handle.
v8::Local<v8::Object> GetObject(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> object,
                                std::string_view name) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> value;
  if (!object->Get(context, gin::StringToSymbol(isolate, name))
           .ToLocal(&value) ||
      !value->IsObject()) {
    return v8::Local<v8::Object>();
  }
  return value.As<v8::Object>();
}

// The functions or events in `list` that are Firefox add-ons' alone.
void HideListed(v8::Local<v8::Context> context,
                const base::ListValue* list,
                v8::Local<v8::Object> object) {
  if (!list) {
    return;
  }
  for (const base::Value& item : *list) {
    const std::string* name =
        item.is_dict() ? item.GetDict().FindString("name") : nullptr;
    if (name && IsFirefoxOnly(item)) {
      Delete(context, object, *name);
    }
  }
}

void HideProperties(v8::Local<v8::Context> context,
                    const base::DictValue& properties,
                    v8::Local<v8::Object> object) {
  for (const auto [name, property] : properties) {
    if (IsFirefoxOnly(property)) {
      Delete(context, object, name);
      continue;
    }
    const base::DictValue* nested =
        property.is_dict() ? property.GetDict().FindDict("properties")
                           : nullptr;
    if (nested && HasMarks(*nested)) {
      v8::Local<v8::Object> child = GetObject(context, object, name);
      if (!child.IsEmpty()) {
        HideProperties(context, *nested, child);
      }
    }
  }
}

// The enum objects of `schema`'s types, such as contextMenus.ContextType.
void HideEnums(v8::Local<v8::Context> context,
               const base::DictValue& schema,
               v8::Local<v8::Object> object) {
  const std::string* api_name = schema.FindString("namespace");
  const base::ListValue* types = schema.FindList("types");
  if (!api_name || !types) {
    return;
  }
  const std::string prefix = *api_name + ".";
  for (const base::Value& type : *types) {
    const std::string* id = type.GetDict().FindString("id");
    const base::ListValue* values = type.GetDict().FindList("enum");
    if (!id || !values) {
      continue;
    }
    std::string_view name(*id);
    if (base::StartsWith(name, prefix)) {
      name.remove_prefix(prefix.size());
    }
    if (IsFirefoxOnly(type)) {
      Delete(context, object, name);
      continue;
    }
    for (const base::Value& value : *values) {
      if (!IsFirefoxOnly(value)) {
        continue;
      }
      v8::Local<v8::Object> enum_object = GetObject(context, object, name);
      const std::string* value_name = value.GetDict().FindString("name");
      if (!enum_object.IsEmpty() && value_name) {
        Delete(context, enum_object, EnumEntryName(*value_name));
      }
    }
  }
}

// The type `ref` names, "tabs.Tab": the bundled schemas name every type with
// its namespace.
const base::DictValue* FindType(std::string_view ref) {
  const size_t dot = ref.rfind('.');
  if (dot == std::string_view::npos) {
    return nullptr;
  }
  const base::DictValue* schema = ExtensionAPI::GetSharedInstance()->GetSchema(
      std::string(ref.substr(0, dot)));
  const base::ListValue* types = schema ? schema->FindList("types") : nullptr;
  if (!types) {
    return nullptr;
  }
  for (const base::Value& type : *types) {
    const std::string* id = type.GetDict().FindString("id");
    if (id && *id == ref) {
      return &type.GetDict();
    }
  }
  return nullptr;
}

// Whether a value of `node` may hold a property marked "firefox_only".
// `seen` holds the types on the way, for a type that holds itself.
bool MayHoldMarks(const base::DictValue& node,
                  std::vector<const base::DictValue*>& seen) {
  if (const std::string* ref = node.FindString("$ref")) {
    const base::DictValue* type = FindType(*ref);
    if (!type || std::ranges::contains(seen, type)) {
      return false;
    }
    seen.push_back(type);
    return MayHoldMarks(*type, seen);
  }
  if (const base::DictValue* properties = node.FindDict("properties")) {
    for (const auto [name, property] : *properties) {
      if (IsFirefoxOnly(property) ||
          (property.is_dict() && MayHoldMarks(property.GetDict(), seen))) {
        return true;
      }
    }
  }
  const base::DictValue* items = node.FindDict("items");
  return items && MayHoldMarks(*items, seen);
}

// MayHoldMarks() of the nodes asked so far. The schemas stay loaded, so a node
// keeps its address. Bindings run on the main thread and on service workers'.
struct MarksCache {
  base::Lock lock;
  std::map<const base::DictValue*, bool> results GUARDED_BY(lock);
};

bool MayHoldMarks(const base::DictValue& node) {
  static base::NoDestructor<MarksCache> cache;
  {
    base::AutoLock lock(cache->lock);
    auto found = cache->results.find(&node);
    if (found != cache->results.end()) {
      return found->second;
    }
  }
  std::vector<const base::DictValue*> seen;
  const bool result = MayHoldMarks(node, seen);
  base::AutoLock lock(cache->lock);
  cache->results[&node] = result;
  return result;
}

void RemoveMarked(v8::Local<v8::Context> context,
                  const base::DictValue& node,
                  v8::Local<v8::Value> value) {
  if (!MayHoldMarks(node)) {
    return;
  }
  if (const std::string* ref = node.FindString("$ref")) {
    if (const base::DictValue* type = FindType(*ref)) {
      RemoveMarked(context, *type, value);
    }
    return;
  }
  if (const base::DictValue* items = node.FindDict("items")) {
    if (value->IsArray()) {
      v8::Local<v8::Array> array = value.As<v8::Array>();
      for (uint32_t i = 0; i < array->Length(); ++i) {
        v8::Local<v8::Value> item;
        if (array->Get(context, i).ToLocal(&item)) {
          RemoveMarked(context, *items, item);
        }
      }
    }
    return;
  }
  const base::DictValue* properties = node.FindDict("properties");
  if (!properties || !value->IsObject()) {
    return;
  }
  v8::Local<v8::Object> object = value.As<v8::Object>();
  for (const auto [name, property] : *properties) {
    if (IsFirefoxOnly(property)) {
      Delete(context, object, name);
    } else if (property.is_dict()) {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      v8::Local<v8::Value> child;
      if (object->Get(context, gin::StringToSymbol(isolate, name))
              .ToLocal(&child)) {
        RemoveMarked(context, property.GetDict(), child);
      }
    }
  }
}

}  // namespace

bool IsFirefoxAddOnContext(v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  return script_context && script_context->extension() &&
         IsFirefoxAddOn(*script_context->extension());
}

void HideFirefoxOnlyMembers(v8::Local<v8::Context> context,
                            const base::DictValue& schema,
                            v8::Local<v8::Object> object) {
  if (IsFirefoxAddOnContext(context) || !HasMarks(schema)) {
    return;
  }
  HideListed(context, schema.FindList("functions"), object);
  HideListed(context, schema.FindList("events"), object);
  if (const base::DictValue* properties = schema.FindDict("properties")) {
    HideProperties(context, *properties, object);
  }
  HideEnums(context, schema, object);
}

void RemoveFirefoxOnlyResults(v8::Local<v8::Context> context,
                              std::string_view name,
                              bool is_event,
                              const v8::LocalVector<v8::Value>& values) {
  const size_t dot = name.rfind('.');
  if (dot == std::string_view::npos || IsFirefoxAddOnContext(context)) {
    return;
  }
  const base::DictValue* schema = ExtensionAPI::GetSharedInstance()->GetSchema(
      std::string(name.substr(0, dot)));
  const base::ListValue* members =
      schema ? schema->FindList(is_event ? "events" : "functions") : nullptr;
  if (!members) {
    return;
  }
  for (const base::Value& member : *members) {
    const std::string* member_name = member.GetDict().FindString("name");
    if (!member_name || *member_name != name.substr(dot + 1)) {
      continue;
    }
    const base::ListValue* parameters =
        is_event ? member.GetDict().FindList("parameters")
                 : member.GetDict().FindListByDottedPath(
                       "returns_async.parameters");
    if (!parameters) {
      return;
    }
    for (size_t i = 0; i < parameters->size() && i < values.size(); ++i) {
      if ((*parameters)[i].is_dict()) {
        RemoveMarked(context, (*parameters)[i].GetDict(), values[i]);
      }
    }
    return;
  }
}

}  // namespace extensions
