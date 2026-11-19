// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_

class Profile;

namespace base {
class FilePath;
}

namespace download {
class DownloadItem;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): when `item` is a
// Firefox add-on, a .xpi, downloaded after a user gesture, installs it and
// returns true. An add-on
// with a Gecko ID installs packed after the install prompt, with the ID from
// its Gecko ID, as a .crx from another site does when the user allows it. One
// without a Gecko ID takes the ID from its AMO signature, as in Firefox
// (firefox_xpi_signature.h). One with neither gets a load error: loading it
// unpacked would skip the prompt.
bool InstallXpiDownload(Profile* profile, const download::DownloadItem& item);

// Whether the download `item`, saved as `filename`, is a .xpi that
// InstallXpiDownload() will install, so the browser doesn't ask where to save
// it: Firefox installs a .xpi from a link without asking, and Android asks
// where to save every download by default. A "Save link as" download is not.
bool IsXpiInstall(Profile* profile,
                  const download::DownloadItem& item,
                  const base::FilePath& filename);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
