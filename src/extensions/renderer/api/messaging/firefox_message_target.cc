// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/api/messaging/firefox_message_target.h"

#include "extensions/common/extension.h"
#include "extensions/common/firefox_gecko_id.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/renderer_extension_registry.h"

namespace extensions {

ExtensionId ExtensionIdForGeckoId(std::string_view target) {
  ExtensionId id = ExtensionIdFromGeckoId(target);
  RendererExtensionRegistry* registry = RendererExtensionRegistry::Get();
  if (id.empty() || registry->Contains(id)) {
    return id;
  }
  for (const ExtensionId& installed : registry->GetIDs()) {
    const Extension* extension = registry->GetByID(installed);
    if (extension && GeckoId(*extension->manifest()->value()) == target) {
      return installed;
    }
  }
  return id;
}

}  // namespace extensions
