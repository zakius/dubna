// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_FIREFOX_MESSAGE_TARGET_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_FIREFOX_MESSAGE_TARGET_H_

#include <string_view>

#include "extensions/common/extension_id.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the ID of the add-on that
// `target`, a Gecko ID given to runtime.sendMessage or runtime.connect, names:
// the installed add-on with that Gecko ID, which has the key's ID if its
// manifest has a key, or else the ID that the Gecko ID gives. Empty when
// `target` isn't a Gecko ID.
ExtensionId ExtensionIdForGeckoId(std::string_view target);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_FIREFOX_MESSAGE_TARGET_H_
