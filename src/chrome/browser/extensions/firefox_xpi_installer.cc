// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_xpi_installer.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/firefox_xpi_signature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/oganesson_strings.h"
#include "components/download/public/common/download_item.h"
#include "crypto/hash.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install_prompt_data.h"
#include "extensions/browser/load_error_reporter.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_gecko_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_message.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#endif

namespace extensions {

namespace {

// Firefox's reason (addon-local-install-error-corrupt-file).
constexpr char16_t kCorruptError[] =
    u"This add-on could not be installed because it appears to be corrupt.";

// The extension pref, true for an add-on installed from an unsigned .xpi.
constexpr char kUnsignedPref[] = "firefox_xpi_unsigned";

// The IDs of the unsigned .xpi files being installed, once for each, for
// their install prompts' warning.
std::multiset<ExtensionId>& UnsignedInstalls() {
  static base::NoDestructor<std::multiset<ExtensionId>> installs;
  return *installs;
}

bool IsUnsignedXpi(content::BrowserContext* context,
                   const ExtensionId& extension_id) {
  bool is_unsigned = false;
  return ExtensionPrefs::Get(context)->ReadPrefAsBoolean(
             extension_id, kUnsignedPref, &is_unsigned) &&
         is_unsigned;
}

// A .xpi that CheckXpi() takes: its add-on's Gecko ID, and whether it is
// unsigned.
struct CheckedXpi {
  std::string gecko_id;
  bool is_unsigned = false;
};

// The add-on unzipped in `unzip_dir` once its AMO signature checks out as in
// Firefox (firefox_xpi_signature.h), or the error, in Firefox's words. Its ID
// is the Gecko ID in the manifest, which has to be the signing certificate's
// common name, as XPIInstall's getSignedStatus() requires, or else that
// common name, which this writes into the manifest, as Firefox takes the ID
// from there. An unsigned one takes its Gecko ID.
base::expected<CheckedXpi, std::u16string> CheckXpi(
    const base::FilePath& unzip_dir) {
  const base::FilePath manifest_path = unzip_dir.Append(kManifestFilename);
  std::string json;
  if (!base::ReadFileToString(manifest_path, &json)) {
    return base::unexpected(kCorruptError);
  }
  std::optional<base::Value> manifest =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!manifest || !manifest->is_dict()) {
    return base::unexpected(kCorruptError);
  }
  std::string gecko_id(GeckoId(manifest->GetDict()));
  std::string signer_id;
  switch (CheckXpiSignature(unzip_dir,
                            Manifest::GetTypeFromManifestValue(
                                manifest->GetDict()) == Manifest::Type::kTheme,
                            &signer_id)) {
    case XpiSignature::kValid:
      break;
    case XpiSignature::kMissing:
      // Without a Gecko ID either, Firefox finds no ID, which makes the file
      // corrupt to it.
      if (gecko_id.empty()) {
        return base::unexpected(kCorruptError);
      }
      return CheckedXpi{.gecko_id = gecko_id, .is_unsigned = true};
    case XpiSignature::kInvalid:
      return base::unexpected(kCorruptError);
  }
  if (!gecko_id.empty()) {
    // AMO signs an ID longer than 64 characters as its SHA-256.
    const std::string signed_id =
        gecko_id.size() > 64
            ? base::HexEncodeLower(crypto::hash::Sha256(gecko_id))
            : gecko_id;
    if (signer_id != signed_id) {
      return base::unexpected(kCorruptError);
    }
    return CheckedXpi{.gecko_id = gecko_id};
  }
  if (ExtensionIdFromGeckoId(signer_id).empty()) {
    return base::unexpected(kCorruptError);
  }
  // The signature's ID goes into the manifest, so the installed add-on has
  // the Gecko ID Firefox gives it: other add-ons reach it by that ID, and it
  // counts as a Firefox add-on (firefox_add_on.h), whose SVG icons, for one,
  // aren't decoded.
  if (!manifest->GetDict().SetByDottedPath("browser_specific_settings.gecko.id",
                                           signer_id)) {
    return base::unexpected(kCorruptError);
  }
  std::optional<std::string> written = base::WriteJson(*manifest);
  if (!written || !base::WriteFile(manifest_path, *written)) {
    return base::unexpected(kCorruptError);
  }
  return CheckedXpi{.gecko_id = signer_id};
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

// Once the add-on of `extension_id` installed, from a signed .xpi or not,
// its extension prefs say which: a signed .xpi installed over an unsigned one
// is verified.
void OnInstalled(base::WeakPtr<Profile> profile,
                 const ExtensionId& extension_id,
                 bool is_unsigned,
                 const std::optional<CrxInstallError>& error) {
  if (is_unsigned) {
    std::multiset<ExtensionId>& installs = UnsignedInstalls();
    auto install = installs.find(extension_id);
    if (install != installs.end()) {
      installs.erase(install);
    }
  }
  if (!profile || error) {
    return;
  }
  ExtensionPrefs::Get(profile.get())
      ->UpdateExtensionPref(extension_id, kUnsignedPref,
                            is_unsigned ? std::optional(base::Value(true))
                                        : std::nullopt);
}

void InstallUnzipped(scoped_refptr<CrxInstaller> installer,
                     base::WeakPtr<Profile> profile,
                     const base::FilePath& xpi,
                     const base::FilePath& unzip_dir,
                     base::expected<CheckedXpi, std::u16string> checked) {
  if (!profile) {
    return;
  }
  // A packed extension takes its ID from its public key, which a .xpi
  // doesn't have. The Gecko ID's bytes stand in for it, so the ID is the
  // one ExtensionIdFromGeckoId() gives, and a "key" in the manifest, which
  // this replaces, can't claim another extension's ID.
  const ExtensionId extension_id =
      checked.has_value() ? ExtensionIdFromGeckoId(checked->gecko_id)
                          : ExtensionId();
  // Without a signature, any site could serve a .xpi of an installed add-on's
  // ID, which would take its storage: only one from an unsigned .xpi may be
  // replaced so.
  if (checked.has_value() && checked->is_unsigned &&
      ExtensionRegistry::Get(profile.get())
          ->GetInstalledExtension(extension_id) &&
      !IsUnsignedXpi(profile.get(), extension_id)) {
    checked = base::unexpected(
        l10n_util::GetStringUTF16(IDS_FIREFOX_XPI_UNSIGNED_OVER_VERIFIED));
  }
  if (checked.has_value()) {
    if (checked->is_unsigned) {
      UnsignedInstalls().insert(extension_id);
    }
    installer->AddInstallerCallback(base::BindOnce(
        &OnInstalled, profile, extension_id, checked->is_unsigned));
    // The installer deletes `unzip_dir` when it is done.
    installer->InstallUnpackedCrx(extension_id,
                                  base::Base64Encode(checked->gecko_id),
                                  unzip_dir);
    return;
  }
  // Not loaded unpacked, even in developer mode: that would install it
  // without a prompt.
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::GetDeletePathRecursivelyCallback(unzip_dir));
  ReportError(profile.get(), xpi, checked.error());
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
      FROM_HERE, base::BindOnce(&CheckXpi, unzip_dir),
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

void AddUnsignedXpiWarning(const Extension* extension,
                           InstallPromptData& prompt) {
  if (!extension || prompt.type() != InstallPromptData::INSTALL_PROMPT ||
      !UnsignedInstalls().contains(extension->id())) {
    return;
  }
  // Firefox says it in the prompt's header (webext-perms-header-unsigned);
  // Chromium's prompt has a list.
  PermissionMessages warning;
  warning.emplace_back(
      l10n_util::GetStringUTF16(IDS_FIREFOX_XPI_UNSIGNED_PROMPT_WARNING),
      PermissionIDSet());
  prompt.AddPermissionMessages(warning);
}

void AddUnverifiedXpiWarning(content::BrowserContext* context,
                             const ExtensionId& extension_id,
                             std::vector<std::string>& warnings) {
  if (IsUnsignedXpi(context, extension_id)) {
    warnings.push_back(l10n_util::GetStringUTF8(IDS_FIREFOX_XPI_UNVERIFIED));
  }
}

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
