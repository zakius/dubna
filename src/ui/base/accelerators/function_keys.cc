// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "ui/base/accelerators/function_keys.h"

#include "base/strings/string_number_conversions.h"

namespace ui {

KeyboardCode FunctionKeyFromToken(std::string_view token) {
  for (int key = VKEY_F1; key <= VKEY_F12; ++key) {
    if (token == FunctionKeyToken(static_cast<KeyboardCode>(key))) {
      return static_cast<KeyboardCode>(key);
    }
  }
  return VKEY_UNKNOWN;
}

std::string FunctionKeyToken(KeyboardCode key_code) {
  if (key_code < VKEY_F1 || key_code > VKEY_F12) {
    return std::string();
  }
  return "F" + base::NumberToString(key_code - VKEY_F1 + 1);
}

}  // namespace ui
