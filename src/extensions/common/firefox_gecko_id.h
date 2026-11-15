// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_GECKO_ID_H_
#define EXTENSIONS_COMMON_FIREFOX_GECKO_ID_H_

#include <string_view>

#include "base/files/file_path.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the extension ID of the
// add-on whose Gecko ID, its ID in Firefox, is `gecko_id`, so that the add-on
// keeps its ID and its storage wherever it is installed from, and other
// add-ons reach it by its Gecko ID. Empty when `gecko_id` isn't an ID that
// Firefox takes: a GUID in braces, or "name@domain". A Chromium ID is
// neither.
ExtensionId ExtensionIdFromGeckoId(std::string_view gecko_id);

// The Gecko ID in `manifest`, in browser_specific_settings.gecko.id or the
// older applications.gecko.id, if Firefox would take it, or empty.
std::string_view GeckoId(const base::DictValue& manifest);

// The extension ID from the Gecko ID in `manifest`, for the add-on in the
// folder `path`, or empty. Empty too for a folder that KeepFolderId() named.
ExtensionId ExtensionIdFromManifest(const base::DictValue& manifest,
                                    const base::FilePath& path);

// An add-on is installed from the folder `path` as `installed_id`. When that
// is the folder's ID, as for an add-on installed unpacked before its ID came
// from its Gecko ID, the add-on keeps it, and its storage, whenever it loads
// from that folder, in this process.
void KeepFolderId(const base::FilePath& path, const ExtensionId& installed_id);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_GECKO_ID_H_
