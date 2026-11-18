// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_items.h"

#include "chrome/browser/extensions/context_menu_helpers.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/common/url_pattern_set.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace extensions {

MenuItem::ContextList FirefoxDefaultMenuContexts(
    const MenuManager& menu_manager,
    const MenuItem::Id* parent_id) {
  const MenuItem* parent =
      parent_id ? menu_manager.GetItemById(*parent_id) : nullptr;
  if (parent) {
    return parent->contexts();
  }
  return MenuItem::ContextList(MenuItem::ALL);
}

bool FirefoxMenuItemMatchesContext(const content::ContextMenuParams& params,
                                   const MenuItem& item) {
  const URLPatternSet& patterns = item.target_url_patterns();
  if (!patterns.is_empty()) {
    bool on_media =
        params.media_type == blink::mojom::ContextMenuDataMediaType::kImage ||
        params.media_type == blink::mojom::ContextMenuDataMediaType::kVideo ||
        params.media_type == blink::mojom::ContextMenuDataMediaType::kAudio;
    bool src_matches = on_media && patterns.MatchesURL(params.src_url);
    bool link_matches =
        !params.link_url.is_empty() && patterns.MatchesURL(params.link_url);
    if (!src_matches && !link_matches) {
      return false;
    }
  }
  // The contexts alone: the patterns matched above.
  return context_menu_helpers::ExtensionContextAndPatternMatch(
      params, item.contexts(), URLPatternSet());
}

}  // namespace extensions
