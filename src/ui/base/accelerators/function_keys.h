// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef UI_BASE_ACCELERATORS_FUNCTION_KEYS_H_
#define UI_BASE_ACCELERATORS_FUNCTION_KEYS_H_

#include <string>
#include <string_view>

#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// Firefox add-ons (dubna's webextensions/README.md): extension shortcuts take
// F1 to F12, as Firefox's do, alone or with any modifiers, Shift included.

// The key that a shortcut token such as "F7" names, or VKEY_UNKNOWN.
KeyboardCode FunctionKeyFromToken(std::string_view token);

// The token for F1 to F12 ("F7"), or an empty string for any other key.
std::string FunctionKeyToken(KeyboardCode key_code);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_FUNCTION_KEYS_H_
