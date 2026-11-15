// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/containers/containers_cleanup.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/containers/container.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace containers {

ContainersCleanup::ContainersCleanup(PrefService* prefs, Delegate* delegate)
    : prefs_(prefs), delegate_(delegate) {}

ContainersCleanup::~ContainersCleanup() = default;

void ContainersCleanup::Run(base::OnceClosure done) {
  if (done) {
    done_callbacks_.push_back(std::move(done));
  }
  if (running_) {
    run_again_ = true;
    return;
  }
  if (Candidates().empty()) {
    Finish();
    return;
  }
  running_ = true;
  delegate_->LoadClosedTabs(base::BindOnce(
      &ContainersCleanup::OnClosedTabsLoaded, weak_factory_.GetWeakPtr()));
}

void ContainersCleanup::Stop() {
  weak_factory_.InvalidateWeakPtrs();
}

std::vector<std::string> ContainersCleanup::Candidates() const {
  std::vector<std::string> candidates;
  for (auto [id, value] : prefs_->GetDict(kContainersUsed)) {
    if (!delegate_->Keeps(id)) {
      candidates.push_back(id);
    }
  }
  return candidates;
}

void ContainersCleanup::OnClosedTabsLoaded() {
  // Taken now, with the tabs: a temporary container made while the delegate
  // answers is no candidate, though no tab of it was counted.
  std::vector<std::string> candidates = Candidates();
  // The closed tabs of deleted containers go; those of temporary ones keep
  // their container.
  std::set<std::string> deleted;
  for (const std::string& id : candidates) {
    if (!id.starts_with(kTemporaryIdPrefix)) {
      deleted.insert(id);
    }
  }
  delegate_->GetInUse(deleted, base::BindOnce(&ContainersCleanup::OnInUse,
                                              weak_factory_.GetWeakPtr(),
                                              std::move(candidates)));
}

void ContainersCleanup::OnInUse(std::vector<std::string> candidates,
                                std::set<std::string> in_use) {
  std::vector<std::string> unused;
  for (const std::string& id : candidates) {
    // Kept again meanwhile, as when sync brings a container back.
    if (!in_use.contains(id) && !delegate_->Keeps(id)) {
      unused.push_back(id);
    }
  }
  // Counted before any deletion starts, so one that answers at once doesn't
  // finish the cleanup.
  pending_deletions_ = unused.size();
  if (!pending_deletions_) {
    Finish();
    return;
  }
  for (const std::string& id : unused) {
    delegate_->DeleteStorage(
        id, base::BindOnce(&ContainersCleanup::OnStorageDeleted,
                           weak_factory_.GetWeakPtr(), id));
  }
}

void ContainersCleanup::OnStorageDeleted(const std::string& id, bool gone) {
  if (gone && !delegate_->Keeps(id)) {
    ScopedDictPrefUpdate(prefs_, kContainersUsed)->Remove(id);
  }
  if (--pending_deletions_ == 0) {
    Finish();
  }
}

void ContainersCleanup::Finish() {
  running_ = false;
  if (run_again_) {
    run_again_ = false;
    Run();
    return;
  }
  for (base::OnceClosure& done : std::exchange(done_callbacks_, {})) {
    std::move(done).Run();
  }
}

}  // namespace containers
