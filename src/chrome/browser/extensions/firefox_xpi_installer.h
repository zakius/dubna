// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_

class Profile;

namespace download {
class DownloadItem;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): when `item` is a
// downloaded Firefox add-on, a .xpi, installs it and returns true. An add-on
// with a Gecko ID installs packed after the install prompt, with the ID from
// its Gecko ID, as a .crx from another site does when the user allows it. One
// without a Gecko ID gets a load error: loading it unpacked would skip the
// prompt. The AMO signature (META-INF/) is ignored.
bool InstallXpiDownload(Profile* profile, const download::DownloadItem& item);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_INSTALLER_H_
