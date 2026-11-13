// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/firefox_sync/containers_adapter.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/containers/container.h"
#include "chrome/browser/containers/traffic_control.h"
#include "components/prefs/pref_service.h"

namespace firefox_sync {

namespace {

using containers::Container;
using containers::TrafficRule;

// Multi-Account Containers' Gecko ID: the area the ops are for.
constexpr char kMacId[] = "@testpilot-containers";

// The host of a rule the add-on holds as a site assignment: enabled, the
// single pattern ".<host>", opening a container of the list. mac.rs checks
// the host itself.
std::string_view SyncedHost(const TrafficRule& rule,
                            const std::set<std::string>& listed) {
  std::string_view filter =
      base::TrimWhitespaceASCII(rule.url_filter, base::TRIM_ALL);
  if (!rule.enabled || !listed.contains(rule.container) ||
      !filter.starts_with('.')) {
    return {};
  }
  return filter.substr(1);
}

std::set<std::string> Ids(const base::ListValue& list) {
  std::set<std::string> ids;
  for (const Container& container : containers::ContainersFromList(list)) {
    ids.insert(container.id);
  }
  return ids;
}

std::optional<TrafficRule> RuleOf(const base::Value& value) {
  return value.is_dict() ? TrafficRule::FromDict(value.GetDict())
                         : std::nullopt;
}

}  // namespace

ContainersAdapter::ContainersAdapter(PrefService* prefs,
                                     base::RepeatingClosure local_changed)
    : prefs_(prefs), local_changed_(std::move(local_changed)) {
  registrar_.Init(prefs_);
  for (const char* pref :
       {containers::kContainersList, containers::kTrafficControlRules}) {
    registrar_.Add(pref, base::BindRepeating(&ContainersAdapter::OnPrefChanged,
                                             base::Unretained(this)));
  }
}

ContainersAdapter::~ContainersAdapter() = default;

// static
bool ContainersAdapter::Available(PrefService* prefs) {
  return prefs->FindPreference(containers::kContainersList) != nullptr;
}

void ContainersAdapter::GetLocal(base::OnceCallback<void(base::Value)> done) {
  base::ListValue list;
  snapshot_containers_.clear();
  for (const Container& container : containers::ContainersFromList(
           prefs_->GetList(containers::kContainersList))) {
    snapshot_containers_.insert(container.id);
    list.Append(container.ToDict());
  }
  base::ListValue rules;
  snapshot_sites_.clear();
  for (const base::Value& value :
       prefs_->GetList(containers::kTrafficControlRules)) {
    if (std::optional<TrafficRule> rule = RuleOf(value)) {
      std::string_view host = SyncedHost(*rule, snapshot_containers_);
      if (!host.empty()) {
        snapshot_sites_.insert(std::string(host));
      }
      rules.Append(rule->ToDict());
    }
  }
  base::DictValue mac;
  mac.Set("containers", std::move(list));
  mac.Set("rules", std::move(rules));
  std::move(done).Run(
      base::Value(base::DictValue().Set(kMacId, std::move(mac))));
}

void ContainersAdapter::Apply(const base::ListValue& ops) {
  base::ListValue list = prefs_->GetList(containers::kContainersList).Clone();
  base::ListValue rules =
      prefs_->GetList(containers::kTrafficControlRules).Clone();
  // The service asks the user before it closes their tabs or deletes their
  // storage.
  base::ListValue sync_deleted =
      prefs_->GetList(containers::kContainersSyncDeleted).Clone();
  size_t changed = 0;
  for (const base::Value& value : ops) {
    const base::DictValue* op = value.GetIfDict();
    const std::string* ext = op ? op->FindString("ext") : nullptr;
    const std::string* kind = op ? op->FindString("op") : nullptr;
    if (!ext || *ext != kMacId || !kind) {
      continue;
    }
    const std::string* id = op->FindString("id");
    const std::string* host = op->FindString("host");
    if (*kind == "container" && id) {
      Container container{.id = *id};
      for (auto [key, field] : {std::pair("name", &container.name),
                                std::pair("icon", &container.icon),
                                std::pair("color", &container.color)}) {
        const std::string* text = op->FindString(key);
        *field = text ? *text : std::string();
      }
      base::Value* existing = nullptr;
      for (base::Value& entry : list) {
        const std::string* entry_id =
            entry.is_dict() ? entry.GetDict().FindString("id") : nullptr;
        if (entry_id && *entry_id == *id) {
          existing = &entry;
          break;
        }
      }
      if (existing) {
        *existing = base::Value(container.ToDict());
      } else if (!snapshot_containers_.contains(*id) &&
                 Container::IsValidId(*id)) {
        list.Append(container.ToDict());
      } else {
        continue;  // Deleted here during the sync.
      }
    } else if (*kind == "delete_container" && id) {
      size_t erased = list.EraseIf([&](const base::Value& entry) {
        const std::string* entry_id =
            entry.is_dict() ? entry.GetDict().FindString("id") : nullptr;
        return entry_id && *entry_id == *id;
      });
      if (erased && !sync_deleted.contains(*id)) {
        sync_deleted.Append(*id);
      }
    } else if ((*kind == "site" || *kind == "delete_site") && host) {
      const std::set<std::string> listed = Ids(list);
      auto synced = [&](const base::Value& entry) {
        std::optional<TrafficRule> rule = RuleOf(entry);
        return rule && SyncedHost(*rule, listed) == *host;
      };
      if (*kind == "delete_site") {
        rules.EraseIf(synced);
      } else {
        const std::string* container = op->FindString("container");
        if (!container) {
          continue;
        }
        auto it = std::find_if(rules.begin(), rules.end(), synced);
        if (it != rules.end()) {
          it->GetDict().Set("container", *container);
        } else if (!snapshot_sites_.contains(*host)) {
          rules.Append(TrafficRule{.url_filter = "." + *host,
                                   .container = *container,
                                   .enabled = true}
                           .ToDict());
        } else {
          continue;  // Deleted here during the sync.
        }
      }
    } else {
      continue;
    }
    ++changed;
  }
  snapshot_containers_.clear();
  snapshot_sites_.clear();
  if (changed) {
    applying_ = true;
    prefs_->SetList(containers::kContainersSyncDeleted,
                    std::move(sync_deleted));
    prefs_->SetList(containers::kContainersList, std::move(list));
    prefs_->SetList(containers::kTrafficControlRules, std::move(rules));
    applying_ = false;
  }
  VLOG(1) << "Firefox Sync: containers: " << changed << " changes applied";
}

void ContainersAdapter::OnPrefChanged() {
  if (!applying_) {
    local_changed_.Run();
  }
}

}  // namespace firefox_sync
