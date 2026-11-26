// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_action_popup.h"

#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "url/origin.h"

namespace extensions {

GURL ResolveFirefoxPopupUrl(const ExtensionFunction& function,
                            std::string_view popup) {
  const Extension& extension = *function.extension();
  const GURL& page = function.source_url();
  // A call whose page isn't known resolves against the root, as Chromium does.
  if (!IsFirefoxAddOn(extension) ||
      !url::IsSameOriginWith(page, extension.url())) {
    return extension.ResolveExtensionURL(popup);
  }
  return Extension::ResolveExtensionURL(page, popup);
}

}  // namespace extensions
