// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/bindings/firefox_menu_shown_tab.h"

#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

void AddFirefoxMenuShownTab(std::string_view event_name,
                            v8::LocalVector<v8::Value>& args) {
  if (event_name != "contextMenus.onShown" || args.size() != 1) {
    return;
  }
  args.push_back(v8::Undefined(v8::Isolate::GetCurrent()));
}

}  // namespace extensions
