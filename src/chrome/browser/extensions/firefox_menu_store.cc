// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_store.h"

#include <optional>
#include <string>
#include <utility>

#include "base/containers/to_value_list.h"
#include "base/values.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

namespace {

// The add-on's extension pref: {"version": ..., "items": [...]}.
constexpr char kFirefoxMenusPref[] = "firefox_menus";
constexpr char kVersionKey[] = "version";
constexpr char kItemsKey[] = "items";

}  // namespace

bool KeepsFirefoxMenus(const Extension& extension) {
  if (!IsFirefoxAddOn(extension) ||
      !BackgroundInfo::HasBackgroundPage(&extension)) {
    return false;
  }
  std::optional<bool> persistent =
      extension.manifest()->FindBoolPath("background.persistent");
  return extension.manifest_version() >= 3 || persistent == false;
}

bool WriteFirefoxMenus(content::BrowserContext* context,
                       const ExtensionId& extension_id,
                       const MenuItem::List& items) {
  // An add-on being uninstalled is in none of the registry's sets: its prefs
  // are about to go.
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetInstalledExtension(extension_id);
  if (!extension || !KeepsFirefoxMenus(*extension)) {
    return false;
  }
  base::DictValue kept;
  kept.Set(kVersionKey, extension->VersionString());
  kept.Set(kItemsKey, base::ToValueList(items, &MenuItem::ToValue));
  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id, kFirefoxMenusPref, base::Value(std::move(kept)));
  return true;
}

std::optional<base::Value> ReadFirefoxMenus(content::BrowserContext* context,
                                            const Extension& extension) {
  if (!KeepsFirefoxMenus(extension)) {
    return std::nullopt;
  }
  const base::DictValue* kept = ExtensionPrefs::Get(context)->ReadPrefAsDict(
      extension.id(), kFirefoxMenusPref);
  if (!kept) {
    return std::nullopt;
  }
  const std::string* version = kept->FindString(kVersionKey);
  const base::ListValue* items = kept->FindList(kItemsKey);
  if (!version || !items || *version != extension.VersionString()) {
    return std::nullopt;
  }
  return base::Value(items->Clone());
}

}  // namespace extensions
