// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINERS_CLEANUP_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINERS_CLEANUP_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class PrefService;

namespace containers {

// Deletes the storage of the containers in kContainersUsed that aren't the
// user's and that nothing uses. What it needs from the browser (tabs, closed
// tabs, storage partitions) is the delegate's, so its unit tests need no
// profile.
class ContainersCleanup {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Whether container `id` keeps its storage though nothing uses it: it is
    // in the user's list, or sync deleted it and the user hasn't chosen yet.
    virtual bool Keeps(const std::string& id) = 0;

    // Calls `loaded` once the closed tabs of the last run are loaded.
    virtual void LoadClosedTabs(base::OnceClosure loaded) = 0;

    // Forgets the closed tabs in the `deleted` containers, then calls
    // `callback` with the containers that open tabs, the last saved session
    // and the remaining closed tabs are in.
    virtual void GetInUse(
        const std::set<std::string>& deleted,
        base::OnceCallback<void(std::set<std::string>)> callback) = 0;

    // Deletes container `id`'s storage, then calls `done` with whether it is
    // off the disk, and not only cleared.
    virtual void DeleteStorage(const std::string& id,
                               base::OnceCallback<void(bool)> done) = 0;
  };

  // `prefs` and `delegate` outlive this.
  ContainersCleanup(PrefService* prefs, Delegate* delegate);
  ContainersCleanup(const ContainersCleanup&) = delete;
  ContainersCleanup& operator=(const ContainersCleanup&) = delete;
  ~ContainersCleanup();

  // Runs the cleanup, or once more after the one running. `done` runs when no
  // cleanup is left to run.
  void Run(base::OnceClosure done = {});

  // Drops the answers of the delegate still to come.
  void Stop();

 private:
  // The used containers that aren't kept.
  std::vector<std::string> Candidates() const;
  void OnClosedTabsLoaded();
  void OnInUse(std::vector<std::string> candidates,
               std::set<std::string> in_use);
  void OnStorageDeleted(const std::string& id, bool gone);
  void Finish();

  const raw_ptr<PrefService> prefs_;
  const raw_ptr<Delegate> delegate_;
  bool running_ = false;
  bool run_again_ = false;
  std::vector<base::OnceClosure> done_callbacks_;
  size_t pending_deletions_ = 0;
  base::WeakPtrFactory<ContainersCleanup> weak_factory_{this};
};

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINERS_CLEANUP_H_
