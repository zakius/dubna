// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_PLATFORM_H_
#define CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_PLATFORM_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace firefox_sync {

class SyncAdapter;

// What Firefox Sync needs from the browser it runs in. dubna has no platform
// code: each product defines CreatePlatform and SyncLibraryPath in its
// //chrome/browser/firefox_sync/platform target, which the browser links.
class Platform {
 public:
  virtual ~Platform() = default;

  // The tabs engine's adapter for the profile's open tabs.
  virtual std::unique_ptr<SyncAdapter> CreateTabsAdapter(
      base::RepeatingClosure local_changed) = 0;
  // A new foreground tab at `url`, or null when there is no window for one.
  virtual content::WebContents* OpenTab(const GURL& url) = 0;
  // The tabs engine's result, the other devices' tabs, for the browser's own
  // views of synced tabs.
  virtual void ShowRemoteTabs(const base::ListValue& devices) {}
  // Hands those views back to Chromium (signed out, or tabs turned off).
  virtual void ClearRemoteTabs() {}
  // The account on the browser profile, like a Google one: `name`, and the
  // picture downloaded from `url` (`picture` holds its bytes, empty for
  // none). With `keep_picture`, only the name changes.
  virtual void ShowAccount(const std::string& name,
                           const std::string& url,
                           std::string picture,
                           bool keep_picture) {}
};

std::unique_ptr<Platform> CreatePlatform(Profile* profile);

// liboganesson_sync.so's path. Blocks.
base::FilePath SyncLibraryPath();

}  // namespace firefox_sync

#endif  // CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_PLATFORM_H_
