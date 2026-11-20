// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/api/storage/firefox_session_size.h"

#include <algorithm>
#include <string>

#include "base/bits.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace extensions {

namespace {

// SpiderMonkey's structured clone (js/src/vm/StructuredClone.cpp) writes
// 8-byte words: a tag word for each value, and a string's characters after
// its tag, padded to a whole word.
constexpr size_t kWord = 8;

// A string is one byte a character when all its characters fit in Latin-1,
// as SpiderMonkey keeps such a string, else two.
size_t StringSize(std::string_view text) {
  std::u16string chars = base::UTF8ToUTF16(text);
  bool latin1 =
      std::ranges::all_of(chars, [](char16_t c) { return c <= 0xFF; });
  size_t bytes = latin1 ? chars.size() : 2 * chars.size();
  return kWord + base::bits::AlignUp(bytes, kWord);
}

// A property key that is an array index, such as "4", is written as a
// number, in one word.
size_t KeySize(std::string_view key) {
  int index;
  if (base::StringToInt(key, &index) && index >= 0 &&
      base::NumberToString(index) == key) {
    return kWord;
  }
  return StringSize(key);
}

size_t ValueSize(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::STRING:
      return StringSize(value.GetString());
    case base::Value::Type::BINARY:
      // Not a value that a script's set() gives, counted as its bytes.
      return kWord + base::bits::AlignUp(value.GetBlob().size(), kWord);
    case base::Value::Type::DICT: {
      // The object's tag, its keys and values, and the end of its keys.
      size_t size = 2 * kWord;
      for (const auto [key, item] : value.GetDict()) {
        size += KeySize(key) + ValueSize(item);
      }
      return size;
    }
    case base::Value::Type::LIST: {
      // The array's tag, each index and value, and the end of its keys.
      size_t size = 2 * kWord;
      for (const base::Value& item : value.GetList()) {
        size += kWord + ValueSize(item);
      }
      return size;
    }
    default:
      // null, a boolean or a number: its tag word holds it, or a double
      // takes a word of its own without a tag.
      return kWord;
  }
}

}  // namespace

size_t FirefoxSessionItemSize(std::string_view key, const base::Value& value) {
  // A structured clone starts with a header word.
  return base::UTF8ToUTF16(key).size() + kWord + ValueSize(value);
}

}  // namespace extensions
