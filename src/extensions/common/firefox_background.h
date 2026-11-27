// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_BACKGROUND_H_
#define EXTENSIONS_COMMON_FIREFOX_BACKGROUND_H_

#include <optional>
#include <string>
#include <vector>

#include "extensions/common/extension_resource.h"
#include "url/gurl.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): the one background that
// Firefox runs, of the manifest's "background" keys. Chromium refuses a
// manifest with more than one; a Firefox add-on (firefox_add_on.h) loads, as
// in Firefox, with Firefox's warnings:
//
// - "page" and "scripts": the page runs.
// - "service_worker" and a page or scripts: the page or scripts run, unless
//   the first entry of "preferred_environment" that Firefox knows
//   ("document" or "service_worker", each other entry a warning) is
//   "service_worker". A manifest without "preferred_environment" gets a
//   warning that names it. These follow Firefox with background service
//   workers enabled, as this browser runs them.
// - "type" other than "classic" or "module" refuses the add-on, also with
//   scripts, which Chromium loads as classic scripts.
//
// `page`, `scripts` and `service_worker` are what BackgroundInfo loaded from
// the manifest; the backgrounds that don't run are cleared. Returns false and
// sets `error` when Firefox refuses the manifest.
bool PickFirefoxBackground(Extension* extension,
                           GURL& page,
                           std::vector<ExtensionResource>& scripts,
                           std::optional<GURL>& service_worker,
                           std::u16string* error);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_BACKGROUND_H_
