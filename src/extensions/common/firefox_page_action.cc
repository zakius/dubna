// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_page_action.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

struct FirefoxPageActionData : public Extension::ManifestData {
  static constexpr char kManifestDataKey[] = "firefox_page_action";

  explicit FirefoxPageActionData(std::unique_ptr<ActionInfo> info)
      : action_info(std::move(info)) {}

  std::unique_ptr<ActionInfo> action_info;
};

}  // namespace

bool HasFirefoxPageAction(const Extension& extension) {
  const Manifest* manifest = extension.manifest();
  if (!manifest->FindKey(manifest_keys::kPageAction) ||
      !IsFirefoxAddOn(extension)) {
    return false;
  }
  return extension.manifest_version() >= 3 ||
         manifest->FindKey(manifest_keys::kAction) ||
         manifest->FindKey(manifest_keys::kBrowserAction);
}

bool HasChromiumPageAction(const Extension& extension) {
  return extension.manifest()->FindKey(manifest_keys::kPageAction) &&
         extension.manifest_version() < 3 && !HasFirefoxPageAction(extension);
}

bool ParseFirefoxPageAction(Extension* extension, std::u16string* error) {
  const base::DictValue* dict =
      extension->manifest()->available_values().FindDict(
          manifest_keys::kPageAction);
  if (!dict) {
    *error = base::ASCIIToUTF16(
        std::string_view(manifest_errors::kInvalidPageAction));
    return false;
  }
  std::vector<InstallWarning> install_warnings;
  std::unique_ptr<ActionInfo> info = ActionInfo::Load(
      extension, ActionInfo::Type::kPage, *dict, &install_warnings, error);
  extension->AddInstallWarnings(std::move(install_warnings));
  if (!info) {
    return false;
  }
  extension->SetManifestData(
      std::make_unique<FirefoxPageActionData>(std::move(info)));
  return true;
}

const ActionInfo* GetFirefoxPageActionInfo(const Extension& extension) {
  const auto* data = extension.GetManifestData<FirefoxPageActionData>();
  return data ? data->action_info.get() : nullptr;
}

}  // namespace extensions
