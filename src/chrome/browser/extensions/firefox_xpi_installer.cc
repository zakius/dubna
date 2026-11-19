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
#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/firefox_xpi_signature.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/load_error_reporter.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/constants.h"
#include "extensions/common/firefox_gecko_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "extensions/browser/install/crx_install_error.h"
#endif

namespace extensions {

namespace {

constexpr char16_t kNoGeckoIdError[] =
    u"This add-on has no Gecko ID (browser_specific_settings.gecko.id) and "
    u"no AMO signature with its ID (META-INF/mozilla.rsa), so it can't be "
    u"installed from a download. Unzip it and load it unpacked in developer "
    u"mode.";

// The Gecko ID in the unzipped add-on's manifest, if Firefox would take it,
// or else the ID in its AMO signature, as Firefox takes it, which this writes
// into the manifest, or "".
std::string ReadGeckoId(const base::FilePath& unzip_dir) {
  const base::FilePath manifest_path = unzip_dir.Append(kManifestFilename);
  std::string json;
  if (!base::ReadFileToString(manifest_path, &json)) {
    return std::string();
  }
  std::optional<base::Value> manifest =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!manifest || !manifest->is_dict()) {
    return std::string();
  }
  std::string gecko_id(GeckoId(manifest->GetDict()));
  if (!gecko_id.empty()) {
    return gecko_id;
  }
  gecko_id = GeckoIdFromSignature(unzip_dir);
  if (gecko_id.empty()) {
    return std::string();
  }
  // The signature's ID goes into the manifest, so the installed add-on has
  // the Gecko ID Firefox gives it: other add-ons reach it by that ID, and it
  // counts as a Firefox add-on (firefox_add_on.h), whose SVG icons, for one,
  // aren't decoded.
  if (!manifest->GetDict().SetByDottedPath(
          "browser_specific_settings.gecko.id", gecko_id)) {
    return std::string();
  }
  std::optional<std::string> written = base::WriteJson(*manifest);
  if (!written || !base::WriteFile(manifest_path, *written)) {
    return std::string();
  }
  return gecko_id;
}

// Reports `error` for the .xpi at `xpi`. Android shows no load error, so there
// the install UI's message says it, as for an add-on that fails to install
// later.
void ReportError(Profile* profile,
                 const base::FilePath& xpi,
                 const std::u16string& error) {
  LoadErrorReporter::GetInstance()->ReportLoadError(xpi, error, profile,
                                                    /*be_noisy=*/true);
#if BUILDFLAG(IS_ANDROID)
  ExtensionInstallUI::Create(profile)->OnInstallFailure(CrxInstallError(
      CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE, error));
#endif
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
  ReportError(profile.get(), xpi, kNoGeckoIdError);
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
    ReportError(profile.get(), xpi, base::UTF8ToUTF16(error));
    return;
  }
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadGeckoId, unzip_dir),
      base::BindOnce(&InstallUnzipped, installer, profile, xpi, unzip_dir));
}

// As for a .crx: not a file the user chose to save ("Save link as"). And only
// after a user gesture, as Firefox asks for, so a page can't open the prompt
// by itself.
bool IsXpiFromGesture(const download::DownloadItem& item,
                      const base::FilePath& filename) {
  return item.GetTargetDisposition() !=
             download::DownloadItem::TARGET_DISPOSITION_PROMPT &&
         item.HasUserGesture() &&
         filename.MatchesExtension(FILE_PATH_LITERAL(".xpi"));
}

}  // namespace

bool IsXpiInstall(Profile* profile,
                  const download::DownloadItem& item,
                  const base::FilePath& filename) {
  return !profile->IsOffTheRecord() && IsXpiFromGesture(item, filename);
}

bool InstallXpiDownload(Profile* profile, const download::DownloadItem& item) {
  // The name the user sees: on Android the download is a content URI, whose
  // own path has no extension.
  if (!IsXpiFromGesture(item, item.GetFileNameToReportUser())) {
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
