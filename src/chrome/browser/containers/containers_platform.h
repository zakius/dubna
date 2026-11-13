// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINERS_PLATFORM_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINERS_PLATFORM_H_

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// What containers need from the browser they run in: its tabs and windows.
// dubna has no platform code: each product defines these in its
// //chrome/browser/containers/platform target, which the browser links.
namespace containers {

struct Container;

// Traffic Control stopped the navigation of `source` to `url` (`transition`,
// from `initiator`) so that it opens in container `container_id`, or outside
// containers when that is empty: opens `url` there in a new tab beside
// `source`, active when `source` is, and closes `source` when it was an empty
// new tab.
void ReopenNavigation(content::WebContents* source,
                      const GURL& url,
                      const std::string& container_id,
                      ui::PageTransition transition,
                      const std::optional<url::Origin>& initiator);

// Calls `callback` with the ids of the containers that `profile`'s open tabs
// and its last saved session (one the user can still restore) are in. Closed
// tabs are dubna's to check.
void GetContainersInUse(
    Profile* profile,
    base::OnceCallback<void(std::set<std::string>)> callback);

// Closes `profile`'s tabs in container `id`, which the user deleted, without
// keeping them as closed tabs. A tab whose page asks to stay may stay.
void CloseTabsInContainer(Profile* profile, const std::string& id);

// Sync deleted `container`, which still has tabs or storage in `profile`:
// asks the user whether to delete it here too, saying how many tabs it has
// open, once the profile has a window and its tabs are restored. Calls
// `callback` with true to delete it, false to keep it, and not at all when
// the user closes the question.
void AskToDeleteSyncedContainer(Profile* profile,
                                const Container& container,
                                base::OnceCallback<void(bool)> callback);

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINERS_PLATFORM_H_
