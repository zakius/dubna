// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_gecko_id.h"

#include <set>
#include <string>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/crx_file/id_util.h"

namespace extensions {

namespace {

// Firefox's pattern for an add-on ID, case-insensitive:
// \{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\} or
// [a-z0-9-._]*@[a-z0-9-._]+
bool IsGeckoId(std::string_view id) {
  if (id.size() == 38 && id.front() == '{' && id.back() == '}') {
    for (size_t i = 1; i < 37; ++i) {
      const bool dash = i == 9 || i == 14 || i == 19 || i == 24;
      if (dash ? id[i] != '-' : !base::IsHexDigit(id[i])) {
        return false;
      }
    }
    return true;
  }
  const size_t at = id.find('@');
  if (at == std::string_view::npos || at + 1 == id.size()) {
    return false;
  }
  for (size_t i = 0; i < id.size(); ++i) {
    const char c = id[i];
    if (i != at && !base::IsAsciiAlphaNumeric(c) && c != '-' && c != '.' &&
        c != '_') {
      return false;
    }
  }
  return true;
}

struct Folders {
  base::Lock lock;
  std::set<base::FilePath> paths GUARDED_BY(lock);
};

// The folders that KeepFolderId() named. Loads run on the UI thread and on
// the extension file task runner.
Folders& KeptFolders() {
  static base::NoDestructor<Folders> folders;
  return *folders;
}

}  // namespace

ExtensionId ExtensionIdFromGeckoId(std::string_view gecko_id) {
  if (!IsGeckoId(gecko_id)) {
    return ExtensionId();
  }
  return crx_file::id_util::GenerateId(gecko_id);
}

std::string_view GeckoId(const base::DictValue& manifest) {
  for (const char* path :
       {"browser_specific_settings.gecko.id", "applications.gecko.id"}) {
    if (const std::string* gecko_id = manifest.FindStringByDottedPath(path)) {
      return IsGeckoId(*gecko_id) ? std::string_view(*gecko_id)
                                  : std::string_view();
    }
  }
  return std::string_view();
}

ExtensionId ExtensionIdFromManifest(const base::DictValue& manifest,
                                    const base::FilePath& path) {
  {
    base::AutoLock lock(KeptFolders().lock);
    if (KeptFolders().paths.contains(path)) {
      return ExtensionId();
    }
  }
  return ExtensionIdFromGeckoId(GeckoId(manifest));
}

void KeepFolderId(const base::FilePath& path, const ExtensionId& installed_id) {
  if (installed_id == crx_file::id_util::GenerateIdForPath(path)) {
    base::AutoLock lock(KeptFolders().lock);
    KeptFolders().paths.insert(path);
  }
}

}  // namespace extensions
