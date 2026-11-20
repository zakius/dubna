// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/api/firefox_i18n_substitutions.h"

#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace extensions {

namespace {

// The substitution of the number in `digits`, which starts at 1.
std::string Substitution(std::string_view digits,
                         v8::Local<v8::Value> substitutions,
                         v8::Local<v8::Context> context) {
  unsigned number = 0;
  if (!base::StringToUint(digits, &number)) {
    return std::string();
  }
  v8::Local<v8::Value> value;
  if (substitutions->IsArray()) {
    // Firefox's `index in substitutions`, so a hole gives nothing.
    v8::Local<v8::Object> array = substitutions.As<v8::Object>();
    if (!array->Has(context, number - 1).FromMaybe(false) ||
        !array->Get(context, number - 1).ToLocal(&value)) {
      return std::string();
    }
  } else if (number == 1 && !substitutions->IsNull()) {
    // Chromium's bindings give null for substitutions that weren't passed.
    value = substitutions;
  } else {
    return std::string();
  }
  v8::String::Utf8Value text(v8::Isolate::GetCurrent(), value);
  return *text ? std::string(*text) : std::string();
}

}  // namespace

std::string FirefoxI18nSubstitutions(const std::string& message,
                                     v8::Local<v8::Value> substitutions,
                                     v8::Local<v8::Context> context) {
  std::string result;
  size_t i = 0;
  while (i < message.size()) {
    size_t dollar = message.find('$', i);
    if (dollar == std::string::npos) {
      result.append(message, i);
      break;
    }
    result.append(message, i, dollar - i);
    size_t next = dollar + 1;
    if (next < message.size() && message[next] == '$') {
      size_t end = message.find_first_not_of('$', next);
      if (end == std::string::npos) {
        end = message.size();
      }
      result.append(message, next, end - next);
      i = end;
    } else if (next < message.size() && message[next] >= '1' &&
               message[next] <= '9') {
      size_t end = message.find_first_not_of("0123456789", next);
      if (end == std::string::npos) {
        end = message.size();
      }
      result += Substitution(std::string_view(message).substr(next, end - next),
                             substitutions, context);
      i = end;
    } else {
      result += '$';
      i = next;
    }
  }
  return result;
}

}  // namespace extensions
