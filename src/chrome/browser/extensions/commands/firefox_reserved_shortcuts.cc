// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/commands/firefox_reserved_shortcuts.h"

#include "chrome/browser/ui/accelerator_table.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

bool FirefoxMayTakeBrowserShortcut(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F11 &&
      accelerator.modifiers() == ui::EF_NONE) {
    return false;
  }
  for (const AcceleratorMapping& entry : GetAcceleratorList()) {
    if (entry.keycode == accelerator.key_code() &&
        entry.modifiers == accelerator.modifiers() &&
        IsReservedCommand(entry.command_id)) {
      return false;
    }
  }
  return true;
}

}  // namespace extensions
