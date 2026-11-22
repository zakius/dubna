// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_menu_target_element.h"

#include <tuple>

#include "extensions/common/extension.h"
#include "extensions/common/firefox_menus_permission.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace extensions {

namespace {

// Firefox's schema error for a targetElementId that isn't a number.
constexpr char kTypeError[] =
    "Type error for parameter targetElementId (Expected integer) for "
    "menus.getTargetElement.";

bool HasGetTargetElement(const ScriptContext& script_context) {
  const Extension* extension = script_context.extension();
  return extension && HasFirefoxMenusPermission(*extension);
}

// getTargetElement(targetElementId). The current context is the function's
// own, the page or content script whose browser.menus it is.
void GetTargetElement(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1 || !info[0]->IsNumber()) {
    isolate->ThrowException(
        v8::Exception::TypeError(gin::StringToV8(isolate, kTypeError)));
    return;
  }
  info.GetReturnValue().SetNull();
  // Every ID that Blink gives is an int.
  if (!info[0]->IsInt32()) {
    return;
  }
  ScriptContext* script_context =
      GetScriptContextFromV8Context(isolate->GetCurrentContext());
  // A worker has no window, so no menu target either.
  blink::WebLocalFrame* frame =
      script_context ? script_context->web_frame() : nullptr;
  if (!frame) {
    return;
  }
  blink::WebElement element = blink::WebElement::FirefoxMenuTarget(
      *frame, info[0].As<v8::Int32>()->Value());
  if (!element.IsNull()) {
    info.GetReturnValue().Set(element.ToV8Value(isolate));
  }
}

void SetGetTargetElement(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Function> function;
  if (!v8::Function::New(context, &GetTargetElement, v8::Local<v8::Value>(), 1,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&function)) {
    return;
  }
  v8::Local<v8::String> name = gin::StringToSymbol(isolate, "getTargetElement");
  function->SetName(name);
  std::ignore = object->CreateDataProperty(context, name, function);
}

// Sets `menus` as the menus of the global object `global_name`, browser or
// chrome, of `context`.
void SetMenus(v8::Local<v8::Context> context,
              const char* global_name,
              v8::Local<v8::Object> menus) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> global;
  if (!context->Global()
           ->Get(context, gin::StringToSymbol(isolate, global_name))
           .ToLocal(&global) ||
      !global->IsObject()) {
    return;
  }
  std::ignore = global.As<v8::Object>()->CreateDataProperty(
      context, gin::StringToSymbol(isolate, "menus"), menus);
}

}  // namespace

void AddFirefoxGetTargetElement(ScriptContext* script_context,
                                std::string_view api_name,
                                v8::Local<v8::Object> binding) {
  if (api_name != "contextMenus" || !HasGetTargetElement(*script_context)) {
    return;
  }
  SetGetTargetElement(script_context->v8_context(), binding);
}

void AddFirefoxContentScriptMenus(ScriptContext* script_context) {
  if (script_context->context_type() != mojom::ContextType::kContentScript ||
      !HasGetTargetElement(*script_context)) {
    return;
  }
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();
  v8::Local<v8::Object> menus = v8::Object::New(isolate);
  SetGetTargetElement(context, menus);
  SetMenus(context, "browser", menus);
  SetMenus(context, "chrome", menus);
}

}  // namespace extensions
