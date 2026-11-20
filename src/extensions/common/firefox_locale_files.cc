// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_locale_files.h"

#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/message_bundle.h"

namespace extensions {

namespace {

// String(value) in JavaScript, for a value read from a JSON file.
std::string JavaScriptString(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::STRING:
      return value.GetString();
    case base::Value::Type::BOOLEAN:
      return value.GetBool() ? "true" : "false";
    case base::Value::Type::INTEGER:
      return base::NumberToString(value.GetInt());
    case base::Value::Type::DOUBLE:
      return base::NumberToString(value.GetDouble());
    case base::Value::Type::LIST: {
      // An array's text joins its items' with commas; null gives nothing.
      std::vector<std::string> items;
      for (const base::Value& item : value.GetList()) {
        items.push_back(item.is_none() ? std::string()
                                       : JavaScriptString(item));
      }
      return base::JoinString(items, ",");
    }
    case base::Value::Type::DICT:
      return "[object Object]";
    case base::Value::Type::NONE:
    case base::Value::Type::BINARY:
      return "null";
  }
  NOTREACHED();
}

}  // namespace

std::string FirefoxPlaceholderContent(const base::Value& placeholder) {
  const base::Value* content =
      placeholder.is_dict()
          ? placeholder.GetDict().Find(MessageBundle::kContentKey)
          : nullptr;
  return content ? JavaScriptString(*content) : std::string();
}

}  // namespace extensions
