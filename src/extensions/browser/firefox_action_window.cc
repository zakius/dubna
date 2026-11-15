// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/firefox_action_window.h"

#include "extensions/browser/extension_action.h"

namespace extensions {

namespace {

int (*g_window_of_tab)(int tab_id) = nullptr;

}  // namespace

int FirefoxWindowOfTab(int tab_id) {
  if (!g_window_of_tab || tab_id == ExtensionAction::kDefaultTabId) {
    return ExtensionAction::kDefaultTabId;
  }
  return g_window_of_tab(tab_id);
}

void SetFirefoxWindowOfTab(int (*window_of_tab)(int tab_id)) {
  g_window_of_tab = window_of_tab;
}

}  // namespace extensions
