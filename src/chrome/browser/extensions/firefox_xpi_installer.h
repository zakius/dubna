// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_

#include <string>
#include <vector>

#include "extensions/common/extension_id.h"

class Profile;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace download {
class DownloadItem;
}

namespace extensions {

class Extension;
class InstallPromptData;

// Firefox add-ons (dubna's webextensions/README.md): when `item` is a
// Firefox add-on, a .xpi, downloaded after a user gesture, installs it and
// returns true. A valid AMO signature (firefox_xpi_signature.h) has a
// certificate whose common name is the add-on's ID: the Gecko ID in its
// manifest has to be that one, and one without a Gecko ID takes it. An
// unsigned .xpi, with neither META-INF/mozilla.rsa nor META-INF/cose.sig,
// installs too, with its Gecko ID, after a warning in the install prompt, as
// Firefox installs it with xpinstall.signatures.required set to false; one
// without a Gecko ID has no ID, as in Firefox. It doesn't install over an
// installed add-on of its ID that isn't from an unsigned .xpi: a site could
// otherwise take that add-on's ID and storage. That is checked before the
// install prompt opens and again when the install happens, after the user
// accepts it (CrxInstaller's install check). It installs packed after the
// install prompt, with the ID from its Gecko ID, as a .crx from another site
// does when the user allows it. One that fails, a broken signature among
// them, gets a load error, with Firefox's reason: loading it unpacked would
// skip the prompt.
bool InstallXpiDownload(Profile* profile, const download::DownloadItem& item);

// Adds the warning of an unsigned .xpi that InstallXpiDownload() installs to
// its install prompt, before the permissions: Mozilla hasn't checked the
// add-on, so it should come only from a source the user trusts. Nothing for
// another `extension` or prompt. ExtensionInstallPrompt calls it as it fills
// `prompt`.
void AddUnsignedXpiWarning(const Extension* extension,
                           InstallPromptData& prompt);

// Adds to `warnings`, the runtime warnings chrome://extensions shows for an
// add-on, that it is not verified when it was installed from an unsigned
// .xpi, as Firefox's add-ons manager says.
void AddUnverifiedXpiWarning(content::BrowserContext* context,
                             const ExtensionId& extension_id,
                             std::vector<std::string>& warnings);

// Whether the download `item`, saved as `filename`, is a .xpi that
// InstallXpiDownload() will install, so the browser doesn't ask where to save
// it: Firefox installs a .xpi from a link without asking, and Android asks
// where to save every download by default. A "Save link as" download is not.
bool IsXpiInstall(Profile* profile,
                  const download::DownloadItem& item,
                  const base::FilePath& filename);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
