// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_COMMANDS_H_
#define EXTENSIONS_COMMON_FIREFOX_COMMANDS_H_

#include <string>

#include "base/values.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): a Firefox add-on's
// commands as Firefox reads them.

// Sets `chromium_commands` to `commands`, the commands of a Firefox add-on's
// manifest, in the form Chromium's CommandsHandler reads. A command's
// suggested_key keeps the platforms that Firefox checks and uses (default,
// mac, linux and windows), and loses the others, android and ios among them,
// which Firefox ignores. In its keys, Command is Ctrl: both are Firefox's
// accel key, which is Command on macOS and Ctrl elsewhere. Returns false with
// `error` for a key with the same modifier twice, Ctrl and Command included,
// which Firefox refuses.
bool ReadFirefoxCommands(const base::DictValue& commands,
                         base::DictValue* chromium_commands,
                         std::u16string* error);

// Sets the description of each command in `commands`, the result of
// commands.getAll(), to the one in `extension`'s manifest, or to null for a
// command without one, as in Firefox.
void SetFirefoxCommandDescriptions(const Extension& extension,
                                   base::ListValue& commands);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_COMMANDS_H_
