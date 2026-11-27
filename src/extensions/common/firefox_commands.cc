// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_commands.h"

#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/accelerators/command_constants.h"

namespace extensions {

namespace {

// The platforms whose keys Firefox checks and uses (its commands.json).
constexpr std::string_view kFirefoxPlatforms[] = {
    ui::kKeybindingPlatformDefault, ui::kKeybindingPlatformMac,
    ui::kKeybindingPlatformLinux, ui::kKeybindingPlatformWin};

// `shortcut` with Command as Ctrl, or nothing when a token repeats.
std::optional<std::string> FirefoxShortcut(std::string_view shortcut) {
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      shortcut, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::set<std::string_view> seen;
  for (std::string_view& token : tokens) {
    if (token == ui::kKeyCommand) {
      token = ui::kKeyCtrl;
    }
    if (!seen.insert(token).second) {
      return std::nullopt;
    }
  }
  return base::JoinString(tokens, "+");
}

// `suggested_key`, of the `index`th command, with Firefox's platforms and
// keys.
bool ReadFirefoxSuggestedKey(base::DictValue& suggested_key,
                             int index,
                             std::u16string* error) {
  base::DictValue keys;
  for (std::string_view platform : kFirefoxPlatforms) {
    const base::Value* key = suggested_key.Find(platform);
    if (!key) {
      continue;
    }
    // Command::Parse refuses a key that isn't a string.
    if (!key->is_string()) {
      keys.Set(platform, key->Clone());
      continue;
    }
    std::optional<std::string> shortcut = FirefoxShortcut(key->GetString());
    if (!shortcut) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidKeyBinding, base::NumberToString(index),
          platform, key->GetString());
      return false;
    }
    keys.Set(platform, *shortcut);
  }
  suggested_key = std::move(keys);
  return true;
}

}  // namespace

bool ReadFirefoxCommands(const base::DictValue& commands,
                         base::DictValue* chromium_commands,
                         std::u16string* error) {
  *chromium_commands = commands.Clone();
  int index = 0;
  for (auto item : *chromium_commands) {
    ++index;
    base::DictValue* command = item.second.GetIfDict();
    base::DictValue* suggested_key =
        command ? command->FindDict(manifest_keys::kSuggestedKey) : nullptr;
    if (suggested_key &&
        !ReadFirefoxSuggestedKey(*suggested_key, index, error)) {
      return false;
    }
  }
  return true;
}

void SetFirefoxCommandDescriptions(const Extension& extension,
                                   base::ListValue& commands) {
  const base::DictValue* manifest_commands =
      extension.manifest()->available_values().FindDict(
          manifest_keys::kCommands);
  for (base::Value& command : commands) {
    base::DictValue& result = command.GetDict();
    const std::string* name = result.FindString("name");
    const base::DictValue* manifest_command =
        manifest_commands && name ? manifest_commands->FindDict(*name)
                                  : nullptr;
    const std::string* description =
        manifest_command
            ? manifest_command->FindString(manifest_keys::kDescription)
            : nullptr;
    if (description) {
      result.Set("description", *description);
    } else {
      result.Set("description", base::Value());
    }
  }
}

}  // namespace extensions
