// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_

#include <optional>
#include <string>
#include <string_view>

#include "content/common/content_export.h"
#include "content/public/browser/storage_partition_config.h"

namespace content {

class NavigationEntry;
class SessionStorageNamespaceHandle;
class WebContents;

// Tab containers (//chrome/browser/containers) keep each container's cookies
// and site storage in a storage partition of this domain, named by the
// container's id. Content knows the domain so that a window a page opens
// without an opener stays in the page's container, as one with an opener does.
inline constexpr char kContainersPartitionDomain[] = "containers";

inline bool IsContainersPartition(const StoragePartitionConfig& config) {
  return config.partition_domain() == kContainersPartitionDomain;
}

// A container's persistent notifications have its partition's name in their
// ids, after this marker (notification_id_generator.cc), so that their click
// and close events reach its service workers: Chromium finds the worker by
// origin, in the site's own partition (notification_event_dispatcher_impl.cc).
inline constexpr char kContainersNotificationMarker = '@';

// The partition name in a persistent notification's id, or nullopt for a
// notification from outside containers.
inline std::optional<std::string> ContainersPartitionOfNotification(
    std::string_view notification_id) {
  std::string_view flags = notification_id.substr(0, notification_id.find('#'));
  size_t marker = flags.find(kContainersNotificationMarker);
  if (marker == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(flags.substr(marker + 1));
}

// The storage partition that `entry` committed in, or nullopt when it hasn't
// committed in this run, as a restored entry that hasn't loaded yet. Defined
// in navigation_entry_impl.cc (dubna's containers.patch): the public
// NavigationEntry doesn't tell its SiteInstance.
CONTENT_EXPORT std::optional<StoragePartitionConfig>
GetCommittedStoragePartitionConfig(NavigationEntry& entry);

// The sessionStorage of the tab `contents`: its container's in a container,
// else the default partition's. Sessions and closed tabs keep this one, as
// Chromium keeps only the default partition's, which a container tab doesn't
// use. Defined in web_contents_impl.cc (dubna's containers.patch): the public
// NavigationController doesn't give a partition's sessionStorage.
CONTENT_EXPORT SessionStorageNamespaceHandle* GetTabSessionStorageNamespace(
    WebContents& contents);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTAINERS_PARTITION_H_
