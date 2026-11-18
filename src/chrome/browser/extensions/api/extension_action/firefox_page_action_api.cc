// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_page_action_api.h"

#include "extensions/browser/extension_action.h"

namespace extensions {

ExtensionFunction::ResponseAction
PageActionIsShownFunction::RunExtensionAction() {
  return RespondNow(WithArguments(extension_action_->GetIsVisible(tab_id_)));
}

}  // namespace extensions
