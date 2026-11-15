// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
//
// Follows Brave's components/containers/core/browser/containers_service.h and
// browser/containers/containers_service_delegate.cc:
// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/containers/container.h"
#include "chrome/browser/containers/containers_cleanup.h"
#include "chrome/browser/containers/traffic_control.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace containers {

// A regular profile's containers and Traffic Control rules, from its prefs
// (container.h), and the storage of containers nothing uses any more.
//
// A container's storage stays while a tab, the last saved session or a
// closed tab is in it. Deleting a container closes its tabs and forgets its
// closed tabs; its storage goes then, or at the next start if a tab stayed
// open or its partition was loaded. A temporary container's storage goes
// once nothing uses it, checked at each start. When sync deletes a container
// that has tabs or storage, the user chooses whether it goes here too or
// comes back; until then its tabs and storage stay.
class ContainersService : public KeyedService,
                          public ContainersCleanup::Delegate,
                          public sessions::TabRestoreServiceObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // The user's list, or a container's name, icon or color, changed.
    virtual void OnContainersChanged() = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit ContainersService(Profile* profile);
  ContainersService(const ContainersService&) = delete;
  ContainersService& operator=(const ContainersService&) = delete;
  ~ContainersService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The user's containers, in their order.
  const std::vector<Container>& GetContainers() const { return containers_; }

  // Container `id`: one of the user's, a temporary one, or a deleted one that
  // a tab is still in.
  std::optional<Container> Get(std::string_view id) const;

  // A new temporary container, named "Temporary <n>" with the lowest n free.
  Container CreateTemporary();

  // Whether sync deleted container `id` and the user hasn't chosen yet
  // whether it goes here too: its tabs restore in it meanwhile.
  bool IsSyncDeleted(std::string_view id) const;

  // Records that container `id` has storage on disk now.
  void MarkUsed(std::string_view id);

  // The container a new tab opens in, as the user set: a container's id, a
  // new temporary container's, or empty.
  std::string NewTabContainer();

  const TrafficRules& traffic_rules() const { return traffic_rules_; }

  // Deletes the storage of the containers that aren't the user's and that
  // nothing uses. `done`, for tests, runs when it has.
  void Cleanup(base::OnceClosure done = {});

  // Deletes the sessionStorage that no tab uses from the partitions of the
  // user's containers, as Chromium's DeleteStaleSessionData() does from the
  // default partition. Call it next to that, once the session is restored or
  // can no longer be: tabs restored before then have recreated theirs.
  static void DeleteStaleSessionData(Profile* profile);

  // KeyedService:
  void Shutdown() override;

 private:
  void OnListChanged();
  // Asks the user about each container sync deleted.
  void AskAboutSyncDeleted();
  void OnSyncDeletedAnswer(const std::string& id, bool delete_it);
  void ForgetSyncDeleted(const std::string& id);
  void OnUsedChanged();
  void OnRulesChanged();
  void OnSessionStorageFound(std::vector<std::string> ids);

  // ContainersCleanup::Delegate:
  bool Keeps(const std::string& id) override;
  void LoadClosedTabs(base::OnceClosure loaded) override;
  void GetInUse(
      const std::set<std::string>& deleted,
      base::OnceCallback<void(std::set<std::string>)> callback) override;
  void DeleteStorage(const std::string& id,
                     base::OnceCallback<void(bool)> done) override;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override {
  }
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar prefs_;
  base::ObserverList<Observer> observers_;
  TrafficRules traffic_rules_;
  // The user's list, parsed once per change: tabs look it up as they paint.
  std::vector<Container> containers_;
  // The ids of the user's list as last seen, to tell which were deleted.
  std::set<std::string> listed_;
  // The containers sync deleted that the user is being asked about.
  std::set<std::string> asking_;
  ContainersCleanup cleanup_;
  // What runs once the closed tabs are loaded, for the cleanup.
  base::OnceClosure closed_tabs_loaded_;
  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      tab_restore_observation_{this};
  base::WeakPtrFactory<ContainersService> weak_factory_{this};
};

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_H_
