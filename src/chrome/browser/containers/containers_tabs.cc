// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/containers/containers_tabs.h"

#include <memory>
#include <utility>

#include "chrome/browser/containers/container.h"
#include "chrome/browser/containers/containers_service.h"
#include "chrome/browser/containers/containers_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/content_serialized_navigation_driver.h"
#include "components/sessions/content/extended_info_handler.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/containers_partition.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace containers {

namespace {

// The key of a page's container in saved sessions and closed tabs.
constexpr char kSessionKey[] = "containers.id";

// NavigationEntry user data: the container of a restored page that hasn't
// loaded yet, so that it is saved again in the same container.
const char kRestoredContainerKey = 0;

struct RestoredContainer : base::SupportsUserData::Data {
  explicit RestoredContainer(std::string id) : id(std::move(id)) {}
  std::unique_ptr<base::SupportsUserData::Data> Clone() override {
    return std::make_unique<RestoredContainer>(id);
  }
  std::string id;
};

class SessionHandler : public sessions::ExtendedInfoHandler {
 public:
  std::string GetExtendedInfo(content::NavigationEntry* entry) const override {
    if (std::optional<content::StoragePartitionConfig> config =
            content::GetCommittedStoragePartitionConfig(*entry)) {
      return ContainerIdOf(*config);
    }
    auto* restored = static_cast<RestoredContainer*>(
        entry->GetUserData(&kRestoredContainerKey));
    return restored ? restored->id : std::string();
  }

  void RestoreExtendedInfo(const std::string& info,
                           content::NavigationEntry* entry) override {
    if (Container::IsValidId(info)) {
      entry->SetUserData(&kRestoredContainerKey,
                         std::make_unique<RestoredContainer>(info));
    }
  }
};

// Makes saved sessions and closed tabs keep each page's container, once, as
// the first tab goes into a container. A profile's service would be too early:
// Chrome creates the first profile before it sets up its navigation driver.
// A restored tab's container is read from its saved navigations, not by this.
void RegisterSessionHandler() {
  static bool registered = false;
  if (registered) {
    return;
  }
  registered = true;
  sessions::ContentSerializedNavigationDriver::GetInstance()
      ->RegisterExtendedInfoHandler(kSessionKey,
                                    std::make_unique<SessionHandler>());
}

// The container `navigation` was in, or empty.
std::string ContainerIdOf(const sessions::SerializedNavigationEntry& navigation) {
  const auto& info = navigation.extended_info_map();
  auto it = info.find(kSessionKey);
  if (it == info.end() || !Container::IsValidId(it->second)) {
    return std::string();
  }
  return it->second;
}

}  // namespace

std::string ContainerIdOf(const content::StoragePartitionConfig& config) {
  return content::IsContainersPartition(config) ? config.partition_name()
                                                : std::string();
}

std::string ContainerIdOf(content::WebContents* contents) {
  return ContainerIdOf(contents->GetSiteInstance()
                           ->GetSecurityPrincipal()
                           .GetStoragePartitionConfig());
}

content::StoragePartitionConfig PartitionOf(content::BrowserContext* context,
                                            std::string_view id) {
  return content::StoragePartitionConfig::Create(
      context, content::kContainersPartitionDomain, std::string(id),
      context->IsOffTheRecord());
}

scoped_refptr<content::SiteInstance> SiteInstanceIn(
    content::BrowserContext* context,
    std::string_view id,
    const GURL& url) {
  RegisterSessionHandler();
  if (ContainersService* service = ContainersServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))) {
    service->MarkUsed(id);
  }
  return content::SiteInstance::CreateForFixedStoragePartition(
      context, url, PartitionOf(context, id));
}

void InheritContainer(content::SiteInstance* source,
                      const GURL& url,
                      scoped_refptr<content::SiteInstance>& site_instance) {
  if (!source) {
    return;
  }
  std::string id =
      ContainerIdOf(source->GetSecurityPrincipal().GetStoragePartitionConfig());
  if (!id.empty()) {
    site_instance = SiteInstanceIn(source->GetBrowserContext(), id, url);
  }
}

std::string ContainerToRestore(content::BrowserContext* context,
                               std::string_view id) {
  ContainersService* service = ContainersServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
  if (id.empty() || !service || id.starts_with(kTemporaryIdPrefix) ||
      service->IsSyncDeleted(id)) {
    return std::string(id);
  }
  for (const Container& container : service->GetContainers()) {
    if (container.id == id) {
      return std::string(id);
    }
  }
  return service->CreateTemporary().id;
}

scoped_refptr<content::SiteInstance> SiteInstanceToRestore(
    content::BrowserContext* context,
    base::span<const sessions::SerializedNavigationEntry> navigations,
    int selected_navigation) {
  // Before the restored pages are built, so they keep their container when
  // saved again.
  RegisterSessionHandler();
  if (selected_navigation < 0 ||
      static_cast<size_t>(selected_navigation) >= navigations.size()) {
    return nullptr;
  }
  // The shown page's container, else any other page's: a page that didn't
  // save one, as an older session's, mustn't take the tab out of it.
  const sessions::SerializedNavigationEntry& navigation =
      navigations[selected_navigation];
  std::string id = ContainerIdOf(navigation);
  for (size_t i = 0; id.empty() && i < navigations.size(); ++i) {
    id = ContainerIdOf(navigations[i]);
  }
  if (id.empty()) {
    return nullptr;
  }
  // A browser page (chrome://settings) loads in its tab's container, as it
  // does when the tab goes to it, so Back reaches the container's sites.
  return SiteInstanceIn(context, ContainerToRestore(context, id),
                        navigation.virtual_url());
}

void MoveSessionStorageToContainer(
    content::SiteInstance* site_instance,
    content::SessionStorageNamespaceHandleMap& map) {
  if (!site_instance) {
    return;
  }
  const content::StoragePartitionConfig& config =
      site_instance->GetSecurityPrincipal().GetStoragePartitionConfig();
  if (!content::IsContainersPartition(config)) {
    return;
  }
  content::BrowserContext* context = site_instance->GetBrowserContext();
  auto saved = map.find(content::StoragePartitionConfig::CreateDefault(context));
  if (saved == map.end() || !saved->second) {
    return;
  }
  // The same id finds the container's saved sessionStorage: after a restart
  // on disk, for a closed tab the one it kept alive.
  scoped_refptr<content::SessionStorageNamespaceHandle> restored =
      context->GetStoragePartition(config)
          ->GetDOMStorageContext()
          ->RecreateSessionStorage(saved->second->id());
  map.clear();
  map[config] = std::move(restored);
}

void AddContainerIds(
    base::span<const sessions::SerializedNavigationEntry> navigations,
    std::set<std::string>& ids) {
  for (const sessions::SerializedNavigationEntry& navigation : navigations) {
    std::string id = ContainerIdOf(navigation);
    if (!id.empty()) {
      ids.insert(std::move(id));
    }
  }
}


}  // namespace containers
