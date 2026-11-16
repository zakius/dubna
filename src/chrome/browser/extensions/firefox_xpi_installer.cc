// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_xpi_installer.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/load_error_reporter.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/constants.h"
#include "extensions/common/firefox_gecko_id.h"

namespace extensions {

namespace {

constexpr char16_t kNoGeckoIdError[] =
    u"This add-on has no Gecko ID (browser_specific_settings.gecko.id), so "
    u"it can't be installed from a download. Unzip it and load it unpacked "
    u"in developer mode.";

// The Gecko ID in the unzipped add-on's manifest, if Firefox would take it,
// or "".
std::string ReadGeckoId(const base::FilePath& unzip_dir) {
  std::string json;
  if (!base::ReadFileToString(unzip_dir.Append(kManifestFilename), &json)) {
    return std::string();
  }
  std::optional<base::Value> manifest =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!manifest || !manifest->is_dict()) {
    return std::string();
  }
  return std::string(GeckoId(manifest->GetDict()));
}

void InstallUnzipped(scoped_refptr<CrxInstaller> installer,
                     base::WeakPtr<Profile> profile,
                     const base::FilePath& xpi,
                     const base::FilePath& unzip_dir,
                     const std::string& gecko_id) {
  if (!profile) {
    return;
  }
  if (!gecko_id.empty()) {
    // A packed extension takes its ID from its public key, which a .xpi
    // doesn't have. The Gecko ID's bytes stand in for it, so the ID is the
    // one ExtensionIdFromGeckoId() gives, and a "key" in the manifest, which
    // this replaces, can't claim another extension's ID. The installer
    // deletes `unzip_dir` when it is done.
    installer->InstallUnpackedCrx(ExtensionIdFromGeckoId(gecko_id),
                                  base::Base64Encode(gecko_id), unzip_dir);
    return;
  }
  // Not loaded unpacked, even in developer mode: that would install it
  // without a prompt.
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::GetDeletePathRecursivelyCallback(unzip_dir));
  LoadErrorReporter::GetInstance()->ReportLoadError(
      xpi, kNoGeckoIdError, profile.get(), /*be_noisy=*/true);
}

void OnUnzipped(scoped_refptr<CrxInstaller> installer,
                base::WeakPtr<Profile> profile,
                const base::FilePath& xpi,
                const base::FilePath& unzip_dir,
                const std::string& error) {
  if (!profile) {
    return;
  }
  if (unzip_dir.empty()) {
    LoadErrorReporter::GetInstance()->ReportLoadError(
        xpi, base::UTF8ToUTF16(error), profile.get(), /*be_noisy=*/true);
    return;
  }
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadGeckoId, unzip_dir),
      base::BindOnce(&InstallUnzipped, installer, profile, xpi, unzip_dir));
}

}  // namespace

bool InstallXpiDownload(Profile* profile, const download::DownloadItem& item) {
  // As for a .crx: not a file the user chose to save ("Save link as").
  if (item.GetTargetDisposition() ==
          download::DownloadItem::TARGET_DISPOSITION_PROMPT ||
      !item.GetTargetFilePath().MatchesExtension(FILE_PATH_LITERAL(".xpi"))) {
    return false;
  }
  scoped_refptr<CrxInstaller> installer =
      download_crx_util::CreateCrxInstaller(profile, item);
  // From any site, as in Firefox, after the install prompt.
  installer->set_off_store_install_allow_reason(
      CrxInstaller::OffStoreInstallAllowedBecausePref);
  // Unzipped next to the unpacked add-ons, as a dropped .zip is. A packed
  // install moves it into the install directory.
  ZipFileInstaller::Create(
      GetExtensionFileTaskRunner(),
      base::BindOnce(&OnUnzipped, installer, profile->GetWeakPtr()))
      ->InstallZipFileToUnpackedExtensionsDir(
          item.GetFullPath(),
          ExtensionRegistrar::Get(profile)->unpacked_install_directory());
  return true;
}

}  // namespace extensions
