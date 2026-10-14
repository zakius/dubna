// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_SAVING_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_SAVING_H_

// For DownloadHistory, which reads the pref.
#include "components/prefs/pref_service.h"  // IWYU pragma: export

namespace prefs {

// While true, DownloadHistory saves no new downloads: they're listed only
// until the browser closes. The download counterpart of
// history.saving_disabled, which leaves downloads alone.
inline constexpr char kSavingDownloadHistoryDisabled[] =
    "oganesson.downloads.saving_disabled";

}  // namespace prefs

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_SAVING_H_
