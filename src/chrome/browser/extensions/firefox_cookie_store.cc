// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_cookie_store.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/containers_partition.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

std::string FirefoxCookieStoreId(content::WebContents* contents) {
  if (contents->GetBrowserContext()->IsOffTheRecord()) {
    return kFirefoxPrivateCookieStore;
  }
  const content::StoragePartitionConfig& config =
      contents->GetSiteInstance()
          ->GetSecurityPrincipal()
          .GetStoragePartitionConfig();
  if (content::IsContainersPartition(config)) {
    return base::StrCat({kFirefoxContainerPrefix, config.partition_name()});
  }
  return kFirefoxDefaultCookieStore;
}

std::optional<std::string> ContainerOfFirefoxCookieStore(
    std::string_view cookie_store_id) {
  if (!base::StartsWith(cookie_store_id, kFirefoxContainerPrefix) ||
      cookie_store_id.size() == std::size(kFirefoxContainerPrefix) - 1) {
    return std::nullopt;
  }
  return std::string(
      cookie_store_id.substr(std::size(kFirefoxContainerPrefix) - 1));
}

}  // namespace extensions
