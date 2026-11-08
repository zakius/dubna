// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {

class NavigationEntry;

// Tab containers (//chrome/browser/containers) keep each container's cookies
// and site storage in a storage partition of this domain, named by the
// container's id. Content knows the domain so that a window a page opens
// without an opener stays in the page's container, as one with an opener does.
inline constexpr char kContainersPartitionDomain[] = "containers";

inline bool IsContainersPartition(const StoragePartitionConfig& config) {
  return config.partition_domain() == kContainersPartitionDomain;
}

// The storage partition that `entry` committed in, or nullopt when it hasn't
// committed in this run, as a restored entry that hasn't loaded yet. Defined
// in navigation_entry_impl.cc (dubna's containers.patch): the public
// NavigationEntry doesn't tell its SiteInstance.
CONTENT_EXPORT std::optional<StoragePartitionConfig>
GetCommittedStoragePartitionConfig(NavigationEntry& entry);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_
