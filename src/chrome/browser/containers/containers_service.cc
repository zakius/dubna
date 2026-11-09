// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
//
// Follows Brave's components/containers/core/browser/containers_service.cc,
// default_containers_list.cc, temporary_container.cc and
// browser/containers/containers_service_delegate.cc:
// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "chrome/browser/containers/containers_service.h"

#include <array>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/containers/containers_platform.h"
#include "chrome/browser/containers/containers_tabs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_types.h"
#include "content/public/browser/containers_partition.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/hash.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace containers {

namespace {

base::ListValue DefaultContainers() {
  // Firefox's.
  const struct {
    const char* id;
    int name;
    const char* icon;
    const char* color;
  } kDefaults[] = {
      {"personal", IDS_CONTAINERS_PERSONAL, "fingerprint", "blue"},
      {"work", IDS_CONTAINERS_WORK, "briefcase", "orange"},
      {"banking", IDS_CONTAINERS_BANKING, "dollar", "green"},
      {"shopping", IDS_CONTAINERS_SHOPPING, "cart", "pink"},
  };
  base::ListValue list;
  for (const auto& container : kDefaults) {
    list.Append(Container{.id = container.id,
                          .name = l10n_util::GetStringUTF8(container.name),
                          .icon = container.icon,
                          .color = container.color}
                    .ToDict());
  }
  return list;
}

// Where Chromium keeps the storage of container `id`'s partition:
// StoragePartitionImplMap::GetStoragePartitionPath(). It keeps a partition's
// folder after the partition is gone, so deleting the storage deletes it.
base::FilePath StoragePath(Profile* profile, const std::string& id) {
  auto hash = crypto::hash::Sha256(id);
  return profile->GetPath()
      .AppendASCII("Storage")
      .AppendASCII("ext")
      .AppendASCII(content::kContainersPartitionDomain)
      .AppendASCII(base::HexEncode(base::span(hash).first<6>()));
}

bool DeleteFolder(const base::FilePath& path) {
  return !base::PathExists(path) || base::DeletePathRecursively(path);
}

void AddContainerIds(const sessions::tab_restore::Entry& entry,
                     std::set<std::string>& ids) {
  std::vector<const sessions::tab_restore::Tab*> tabs;
  switch (entry.type) {
    case sessions::tab_restore::TAB:
      tabs.push_back(static_cast<const sessions::tab_restore::Tab*>(&entry));
      break;
    case sessions::tab_restore::WINDOW:
      for (const auto& tab :
           static_cast<const sessions::tab_restore::Window&>(entry).tabs) {
        tabs.push_back(tab.get());
      }
      break;
    case sessions::tab_restore::GROUP:
      for (const auto& tab :
           static_cast<const sessions::tab_restore::Group&>(entry).tabs) {
        tabs.push_back(tab.get());
      }
      break;
    case sessions::tab_restore::SPLIT:
      for (const auto& tab :
           static_cast<const sessions::tab_restore::Split&>(entry).tabs) {
        tabs.push_back(tab.get());
      }
      break;
  }
  for (const sessions::tab_restore::Tab* tab : tabs) {
    containers::AddContainerIds(tab->navigations, ids);
  }
}

}  // namespace

// static
void ContainersService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kContainersList, DefaultContainers());
  registry->RegisterDictionaryPref(kContainersUsed);
  registry->RegisterListPref(kTrafficControlRules);
  registry->RegisterStringPref(kNewTabContainer, std::string());
}

ContainersService::ContainersService(Profile* profile) : profile_(profile) {
  prefs_.Init(profile->GetPrefs());
  prefs_.Add(kContainersList,
             base::BindRepeating(&ContainersService::OnListChanged,
                                 base::Unretained(this)));
  prefs_.Add(kContainersUsed,
             base::BindRepeating(&ContainersService::OnUsedChanged,
                                 base::Unretained(this)));
  prefs_.Add(kTrafficControlRules,
             base::BindRepeating(&ContainersService::OnRulesChanged,
                                 base::Unretained(this)));
  containers_ =
      ContainersFromList(profile->GetPrefs()->GetList(kContainersList));
  for (const Container& container : containers_) {
    listed_.insert(container.id);
  }
  OnRulesChanged();
  // After startup's own tasks, such as restoring the last session.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ContainersService::Cleanup,
                                weak_factory_.GetWeakPtr(), base::OnceClosure()));
}

ContainersService::~ContainersService() = default;

void ContainersService::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
  tab_restore_observation_.Reset();
  prefs_.RemoveAll();
}

void ContainersService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContainersService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<Container> ContainersService::Get(std::string_view id) const {
  for (const Container& container : containers_) {
    if (container.id == id) {
      return container;
    }
  }
  const base::DictValue* used =
      profile_->GetPrefs()->GetDict(kContainersUsed).FindDict(id);
  return used ? Container::FromDict(used->Clone().Set("id", id))
              : std::nullopt;
}

Container ContainersService::CreateTemporary() {
  std::set<std::string> names;
  for (auto [id, value] : profile_->GetPrefs()->GetDict(kContainersUsed)) {
    if (const std::string* name =
            value.is_dict() ? value.GetDict().FindString("name") : nullptr) {
      names.insert(*name);
    }
  }
  std::string name;
  for (int n = 1; name.empty() || names.contains(name); ++n) {
    name = l10n_util::GetStringFUTF8(IDS_CONTAINERS_TEMPORARY_NAME,
                                     base::NumberToString16(n));
  }
  static constexpr auto kColors = std::to_array<const char*>(
      {"blue", "turquoise", "green", "yellow", "orange", "red", "pink",
       "purple"});
  Container container{
      .id = kTemporaryIdPrefix +
            base::Uuid::GenerateRandomV4().AsLowercaseString(),
      .name = name,
      .icon = "circle",
      .color = base::RandomChoice(kColors)};
  ScopedDictPrefUpdate(profile_->GetPrefs(), kContainersUsed)
      ->Set(container.id, container.ToDict());
  return container;
}

std::string ContainersService::NewTabContainer() {
  const std::string& id = profile_->GetPrefs()->GetString(kNewTabContainer);
  if (id == kTemporaryTarget) {
    return CreateTemporary().id;
  }
  return listed_.contains(id) ? id : std::string();
}

void ContainersService::MarkUsed(std::string_view id) {
  std::optional<Container> container = Get(id);
  if (!container) {
    // A container this profile doesn't know, as from a session another
    // browser saved: it gets a name from its id.
    container = Container{.id = std::string(id),
                          .name = std::string(id.substr(0, 8)),
                          .icon = "circle",
                          .color = "toolbar"};
  }
  const base::DictValue* used =
      profile_->GetPrefs()->GetDict(kContainersUsed).FindDict(id);
  base::DictValue snapshot = container->ToDict();
  if (!used || *used != snapshot) {
    ScopedDictPrefUpdate(profile_->GetPrefs(), kContainersUsed)
        ->Set(id, std::move(snapshot));
  }
}

void ContainersService::OnListChanged() {
  containers_ =
      ContainersFromList(profile_->GetPrefs()->GetList(kContainersList));
  std::set<std::string> listed;
  for (const Container& container : containers_) {
    listed.insert(container.id);
    // Tabs in a container the user renamed show the new name.
    if (profile_->GetPrefs()->GetDict(kContainersUsed).contains(container.id)) {
      MarkUsed(container.id);
    }
  }
  bool deleted = false;
  for (const std::string& id : listed_) {
    if (!listed.contains(id)) {
      CloseTabsInContainer(profile_, id);
      deleted = true;
    }
  }
  listed_ = std::move(listed);
  observers_.Notify(&Observer::OnContainersChanged);
  if (deleted) {
    Cleanup();
  }
}

void ContainersService::OnUsedChanged() {
  observers_.Notify(&Observer::OnContainersChanged);
}

void ContainersService::OnRulesChanged() {
  traffic_rules_ =
      TrafficRules(profile_->GetPrefs()->GetList(kTrafficControlRules));
}

std::vector<std::string> ContainersService::CleanupCandidates() const {
  std::vector<std::string> candidates;
  for (auto [id, value] : profile_->GetPrefs()->GetDict(kContainersUsed)) {
    if (!listed_.contains(id)) {
      candidates.push_back(id);
    }
  }
  return candidates;
}

void ContainersService::Cleanup(base::OnceClosure done) {
  if (done) {
    cleaned_callbacks_.push_back(std::move(done));
  }
  if (cleaning_) {
    clean_again_ = true;
    return;
  }
  if (CleanupCandidates().empty()) {
    FinishCleanup();
    return;
  }
  cleaning_ = true;
  sessions::TabRestoreService* tab_restore =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (tab_restore && !tab_restore->IsLoaded()) {
    if (!tab_restore_observation_.IsObserving()) {
      tab_restore_observation_.Observe(tab_restore);
    }
    tab_restore->LoadTabsFromLastSession();
    return;
  }
  CheckClosedTabs();
}

void ContainersService::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  tab_restore_observation_.Reset();
  if (cleaning_) {
    CheckClosedTabs();
  }
}

void ContainersService::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_observation_.Reset();
}

void ContainersService::CheckClosedTabs() {
  // Taken now, with the tabs: a temporary container made while the platform
  // answers is no candidate, though no tab of it was counted.
  std::vector<std::string> candidates = CleanupCandidates();
  // The closed tabs of deleted containers go; those of temporary ones keep
  // their container.
  std::set<std::string> deleted;
  for (const std::string& id : candidates) {
    if (!id.starts_with(kTemporaryIdPrefix)) {
      deleted.insert(id);
    }
  }
  std::set<std::string> in_use;
  if (sessions::TabRestoreService* tab_restore =
          TabRestoreServiceFactory::GetForProfile(profile_)) {
    if (!deleted.empty()) {
      tab_restore->DeleteNavigationEntries(base::BindRepeating(
          [](const std::set<std::string>& deleted,
             const sessions::SerializedNavigationEntry& navigation) {
            std::set<std::string> ids;
            containers::AddContainerIds(base::span_from_ref(navigation), ids);
            return !ids.empty() && deleted.contains(*ids.begin());
          },
          deleted));
    }
    for (const auto& entry : tab_restore->entries()) {
      AddContainerIds(*entry, in_use);
    }
  }
  GetContainersInUse(
      profile_, base::BindOnce(
                    [](base::WeakPtr<ContainersService> service,
                       std::vector<std::string> candidates,
                       std::set<std::string> closed_tabs,
                       std::set<std::string> in_use) {
                      if (service) {
                        in_use.merge(closed_tabs);
                        service->OnInUse(std::move(candidates),
                                         std::move(in_use));
                      }
                    },
                    weak_factory_.GetWeakPtr(), std::move(candidates),
                    std::move(in_use)));
}

void ContainersService::OnInUse(std::vector<std::string> candidates,
                                std::set<std::string> in_use) {
  for (const std::string& id : candidates) {
    // Listed again meanwhile, as when sync brings a container back.
    if (in_use.contains(id) || listed_.contains(id)) {
      continue;
    }
    ++pending_deletions_;
    base::OnceCallback<void(bool)> deleted = base::BindOnce(
        &ContainersService::OnStorageDeleted, weak_factory_.GetWeakPtr(), id);
    base::FilePath path = StoragePath(profile_, id);
    auto delete_folder = base::BindOnce(
        [](base::FilePath path, base::OnceCallback<void(bool)> deleted) {
          base::ThreadPool::PostTaskAndReplyWithResult(
              FROM_HERE, {base::MayBlock()},
              base::BindOnce(&DeleteFolder, path), std::move(deleted));
        },
        path, std::move(deleted));
    // A partition loaded in this run first clears what it holds in memory.
    if (content::StoragePartition* partition = profile_->GetStoragePartition(
            PartitionOf(profile_, id), /*can_create=*/false)) {
      partition->ClearData(content::StoragePartition::REMOVE_DATA_MASK_ALL,
                           /*filter_builder=*/nullptr,
                           /*storage_key_policy_matcher=*/{},
                           network::mojom::CookieDeletionFilter::New(),
                           /*perform_storage_cleanup=*/true, base::Time(),
                           base::Time::Max(), std::move(delete_folder));
    } else {
      std::move(delete_folder).Run();
    }
  }
  if (!pending_deletions_) {
    FinishCleanup();
  }
}

void ContainersService::OnStorageDeleted(const std::string& id, bool success) {
  if (success && !listed_.contains(id)) {
    ScopedDictPrefUpdate(profile_->GetPrefs(), kContainersUsed)->Remove(id);
  }
  if (--pending_deletions_ == 0) {
    FinishCleanup();
  }
}

void ContainersService::FinishCleanup() {
  cleaning_ = false;
  if (clean_again_) {
    clean_again_ = false;
    Cleanup();
    return;
  }
  for (base::OnceClosure& done : std::exchange(cleaned_callbacks_, {})) {
    std::move(done).Run();
  }
}

}  // namespace containers
