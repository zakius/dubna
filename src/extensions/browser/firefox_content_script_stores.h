// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_CONTENT_SCRIPT_STORES_H_
#define EXTENSIONS_BROWSER_FIREFOX_CONTENT_SCRIPT_STORES_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// contentScripts.register() (api/scripting/firefox_content_scripts.h) takes a
// cookieStoreId, and its scripts then run only in the tabs of those cookie
// stores (extensions/common/firefox_cookie_store.h). A renderer process holds
// the pages of one store: a container is a storage partition, and private
// windows have a profile of their own. So UserScriptLoader sends each process
// only the scripts of its store. UserScript has no field for the stores, so
// they are in the script's ID, which goes wherever the script goes.
namespace extensions::firefox_content_scripts {

// A new ID for contentScripts scripts that run in the cookie stores `stores`,
// Firefox's cookieStoreIds, or in all of them if `stores` is empty.
std::string NewScriptId(const std::vector<std::string>& stores);

// Whether `id`, with its dynamic content script prefix, is a contentScripts
// script.
bool IsFirefoxScriptId(std::string_view id);

// Returns false if every script of `scripts` runs in `process`. Otherwise
// sets `region` to the scripts that do, serialized, and returns true.
bool ScriptsForProcess(content::RenderProcessHost& process,
                       const UserScriptList& scripts,
                       base::ReadOnlySharedMemoryRegion& region);

// Whether the profile of `context` has the container `id`
// (//chrome/browser/containers): one of the user's, a temporary one, or a
// deleted one that a tab is still in, as contextualIdentities.get() finds.
// False until SetHasContainer() is called, as in a Firefox without
// containers.
bool HasContainer(content::BrowserContext* context, std::string_view id);

// Sets the function that HasContainer() asks, which needs the browser's
// containers. The contextualIdentities API sets it with each profile
// (contextual_identities_api.cc).
void SetHasContainer(bool (*has_container)(content::BrowserContext* context,
                                           std::string_view id));

}  // namespace extensions::firefox_content_scripts

#endif  // EXTENSIONS_BROWSER_FIREFOX_CONTENT_SCRIPT_STORES_H_
