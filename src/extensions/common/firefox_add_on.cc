// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_add_on.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

bool IsFirefoxAddOn(const Extension& extension) {
  const base::DictValue* manifest = extension.manifest()->value();
  return manifest->FindDictByDottedPath("browser_specific_settings.gecko") ||
         manifest->FindDictByDottedPath("applications.gecko");
}

}  // namespace extensions
