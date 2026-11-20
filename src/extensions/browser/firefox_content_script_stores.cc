// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/firefox_content_script_stores.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/containers_partition.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/firefox_cookie_store.h"

namespace extensions::firefox_content_scripts {

namespace {

// Follows the dynamic content script prefix. The scripting API refuses IDs
// that start with "_", so none of its IDs looks like this.
constexpr char kIdPrefix[] = "_firefox_";

// Between the unique part of an ID and each of its stores. No cookieStoreId
// that register() takes has one.
constexpr char kStoreSeparator[] = " ";

bool (*g_has_container)(content::BrowserContext* context,
                        std::string_view id) = nullptr;

// The cookie store of the pages in `process`, as Firefox's cookieStoreId.
std::string StoreOfProcess(content::RenderProcessHost& process) {
  if (process.GetBrowserContext()->IsOffTheRecord()) {
    return kFirefoxPrivateCookieStore;
  }
  const content::StoragePartitionConfig& config =
      process.GetStoragePartition()->GetConfig();
  if (content::IsContainersPartition(config)) {
    return base::StrCat({kFirefoxContainerPrefix, config.partition_name()});
  }
  return kFirefoxDefaultCookieStore;
}

// Whether the script `id` runs in the cookie store `store`.
bool RunsInStore(std::string_view id, std::string_view store) {
  if (!IsFirefoxScriptId(id)) {
    return true;
  }
  std::vector<std::string_view> parts = base::SplitStringPiece(
      id, kStoreSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // A script for every store has no stores in its ID.
  if (parts.size() == 1) {
    return true;
  }
  for (size_t i = 1; i < parts.size(); ++i) {
    if (parts[i] == store) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::string NewScriptId(const std::vector<std::string>& stores) {
  std::string id =
      base::StrCat({UserScript::kDynamicContentScriptPrefix, kIdPrefix,
                    base::Uuid::GenerateRandomV4().AsLowercaseString()});
  for (const std::string& store : stores) {
    base::StrAppend(&id, {kStoreSeparator, store});
  }
  return id;
}

bool IsFirefoxScriptId(std::string_view id) {
  // Without a new string: the loader asks this of every script it sends.
  std::string_view dynamic_prefix = UserScript::kDynamicContentScriptPrefix;
  return base::StartsWith(id, dynamic_prefix) &&
         base::StartsWith(id.substr(dynamic_prefix.size()), kIdPrefix);
}

bool ScriptsForProcess(content::RenderProcessHost& process,
                       const UserScriptList& scripts,
                       base::ReadOnlySharedMemoryRegion& region) {
  const std::string store = StoreOfProcess(process);
  bool all_run = true;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (!RunsInStore(script->id(), store)) {
      all_run = false;
      break;
    }
  }
  if (all_run) {
    return false;
  }
  // Copied with their content, as UserScriptLoader::SendUpdate() copies the
  // scripts of a Controlled Frame.
  UserScriptList kept;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (!RunsInStore(script->id(), store)) {
      continue;
    }
    std::unique_ptr<UserScript> copy = UserScript::CopyMetadataFrom(*script);
    for (size_t i = 0; i < script->js_scripts().size(); ++i) {
      copy->js_scripts()[i]->set_content(
          std::string(script->js_scripts()[i]->GetContent()));
    }
    for (size_t i = 0; i < script->css_scripts().size(); ++i) {
      copy->css_scripts()[i]->set_content(
          std::string(script->css_scripts()[i]->GetContent()));
    }
    kept.push_back(std::move(copy));
  }
  region = UserScriptLoader::Serialize(kept);
  return true;
}

bool HasContainer(content::BrowserContext* context, std::string_view id) {
  return g_has_container && g_has_container(context, id);
}

void SetHasContainer(bool (*has_container)(content::BrowserContext* context,
                                           std::string_view id)) {
  g_has_container = has_container;
}

}  // namespace extensions::firefox_content_scripts
