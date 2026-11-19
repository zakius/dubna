// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_UI_EXTENSIONS_INSTALL_FAILURE_MESSAGE_ANDROID_H_
#define CHROME_BROWSER_UI_EXTENSIONS_INSTALL_FAILURE_MESSAGE_ANDROID_H_

#include <string>

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// The tab of `profile` that the user sees: the active tab of its active tab
// model, or else of any of its tab models. GetWebContentsForProfile() of
// extension_install_ui_android.cc, without an active window, takes a tab
// model's first tab, which may be any of many: the post-install dialog then
// waited on a tab in the background and held back every message, such as
// "File downloaded", while it was there.
content::WebContents* GetVisibleTab(Profile* profile);

// Firefox add-ons (dubna's webextensions/README.md): on Android, an extension
// that fails to install, such as a downloaded .xpi with a broken manifest,
// says why in a message on the tab of `profile` that the user sees, as
// desktop's infobar does. The message goes when the tab navigates.
void ShowInstallFailureMessage(Profile* profile, const std::u16string& error);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_INSTALL_FAILURE_MESSAGE_ANDROID_H_
