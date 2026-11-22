// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_view_types.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

const char* FirefoxMenuViewType(content::WebContents* web_contents) {
  switch (GetViewType(web_contents)) {
    case mojom::ViewType::kTabContents:
      return "tab";
    case mojom::ViewType::kExtensionPopup:
      return "popup";
    case mojom::ViewType::kExtensionSidePanel:
      return "sidebar";
    default:
      return nullptr;
  }
}

void SetFirefoxMenuViewTypes(
    const std::vector<api::context_menus::ViewType>& view_types,
    MenuItem& item) {
  std::vector<std::string> names;
  for (api::context_menus::ViewType view_type : view_types) {
    names.push_back(api::context_menus::ToString(view_type));
  }
  item.set_firefox_view_types(std::move(names));
}

void ContextMenuMatcher::SetFirefoxPageView(
    content::WebContents* web_contents) {
  const char* view_type = FirefoxMenuViewType(web_contents);
  firefox_view_type_ = view_type ? view_type : "";
}

bool ContextMenuMatcher::InFirefoxView(const MenuItem& item) const {
  if (!item.firefox_add_on() || item.firefox_view_types().empty()) {
    return true;
  }
  // A menu without a viewType has an empty one, which no item names.
  return std::ranges::contains(item.firefox_view_types(), firefox_view_type_);
}

}  // namespace extensions
