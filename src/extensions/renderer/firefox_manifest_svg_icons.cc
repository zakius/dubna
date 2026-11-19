// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_manifest_svg_icons.h"

#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

void DrawFirefoxManifestSvgIcons(ScriptContext& context) {
  const Extension* extension = context.extension();
  if (context.context_type() != mojom::ContextType::kPrivilegedExtension ||
      !extension || !IsFirefoxAddOn(*extension) ||
      !BackgroundInfo::HasBackgroundPage(extension) ||
      context.url() != BackgroundInfo::GetBackgroundURL(extension)) {
    return;
  }
  v8::HandleScope handle_scope(context.isolate());
  // CallModuleMethodSafe() calls only into a module that is loaded.
  ModuleSystem* module_system = context.module_system();
  if (module_system->Require("firefoxSvgIcons").IsEmpty()) {
    return;
  }
  module_system->CallModuleMethodSafe("firefoxSvgIcons", "drawManifestIcons");
}

}  // namespace extensions
