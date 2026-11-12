// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINERS_TABS_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINERS_TABS_H_

#include <set>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/session_storage_namespace_handle.h"

class GURL;

namespace content {
class BrowserContext;
class SiteInstance;
class StoragePartitionConfig;
class WebContents;
}  // namespace content

namespace sessions {
class SerializedNavigationEntry;
}

// Which container a tab is in. A tab's pages stay in the storage partition
// its WebContents starts in: Chromium keeps every navigation of a SiteInstance
// with a fixed storage partition in that partition. So a tab goes into a
// container only when it is created, and these give the SiteInstance to
// create it with.
namespace containers {

// The id of the container of `config`, or empty outside containers.
std::string ContainerIdOf(const content::StoragePartitionConfig& config);
// The id of the container `contents` shows its pages in, or empty.
std::string ContainerIdOf(content::WebContents* contents);

// The storage partition of container `id` in `context`.
content::StoragePartitionConfig PartitionOf(content::BrowserContext* context,
                                            std::string_view id);

// A SiteInstance for a new tab that loads `url` in container `id`. Records
// that the container has storage now (ContainersService::MarkUsed()). Every
// tab gets into a container through this, which also makes sessions and
// closed tabs keep each page's container (a sessions::ExtendedInfoHandler).
scoped_refptr<content::SiteInstance> SiteInstanceIn(
    content::BrowserContext* context,
    std::string_view id,
    const GURL& url);

// For a new tab without an opener that a page in `source` opens on `url`, as
// a link to a new tab does: when `source` is in a container, replaces
// `site_instance` with one in that container, so the link stays in it.
void InheritContainer(content::SiteInstance* source,
                      const GURL& url,
                      scoped_refptr<content::SiteInstance>& site_instance);

// The container to restore a tab of container `id` in: `id` while it is one
// of the user's or a temporary one, else a new temporary container, so a
// deleted container's storage isn't used again. Empty stays empty.
std::string ContainerToRestore(content::BrowserContext* context,
                               std::string_view id);

// The SiteInstance to restore a tab with, from its saved `navigations` and
// the one it shows: in the container its pages were in, or null for the
// default. A browser page (chrome://) in a container tab restores in the
// container too, and a deleted container's pages in a new temporary one.
scoped_refptr<content::SiteInstance> SiteInstanceToRestore(
    content::BrowserContext* context,
    base::span<const sessions::SerializedNavigationEntry> navigations,
    int selected_navigation);

// Moves the saved sessionStorage of a tab restored with `site_instance` from
// the default partition, where Chromium restores it, to the tab's container,
// whose pages use it. Leaves `map` as it is outside containers.
void MoveSessionStorageToContainer(
    content::SiteInstance* site_instance,
    content::SessionStorageNamespaceHandleMap& map);

// Adds the ids of the containers `navigations` were in to `ids`.
void AddContainerIds(
    base::span<const sessions::SerializedNavigationEntry> navigations,
    std::set<std::string>& ids);

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINERS_TABS_H_
