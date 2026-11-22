// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_last_error.h"

#include <tuple>

#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

void AddFirefoxNullLastError(ScriptContext* script_context,
                             std::string_view api_name,
                             v8::Local<v8::Object> binding) {
  const Extension* extension = script_context->extension();
  if (api_name != "runtime" || !extension || !IsFirefoxAddOn(*extension)) {
    return;
  }
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Name> names[] = {gin::StringToSymbol(isolate, "lastError")};
  v8::Local<v8::Value> values[] = {v8::Null(isolate)};
  v8::Local<v8::Object> holder =
      v8::Object::New(isolate, binding->GetPrototype(), names, values, 1);
  std::ignore = binding->SetPrototype(script_context->v8_context(), holder);
}

}  // namespace extensions
