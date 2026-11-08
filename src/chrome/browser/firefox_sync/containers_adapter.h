// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_FIREFOX_SYNC_CONTAINERS_ADAPTER_H_
#define CHROME_BROWSER_FIREFOX_SYNC_CONTAINERS_ADAPTER_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/firefox_sync/sync_adapter.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace firefox_sync {

// `extension-storage`: Multi-Account Containers' storage.sync area, as the
// profile's containers and Traffic Control rules (the containers.list and
// containers.traffic_control prefs). The library's extension_storage.rs and
// mac.rs define the snapshot and the ops.
class ContainersAdapter : public SyncAdapter {
 public:
  ContainersAdapter(PrefService* prefs, base::RepeatingClosure local_changed);
  ~ContainersAdapter() override;

  // Whether the browser has containers: their prefs are registered.
  static bool Available(PrefService* prefs);

  // SyncAdapter:
  void GetLocal(base::OnceCallback<void(base::Value)> done) override;
  void Apply(const base::ListValue& ops) override;

 private:
  void OnPrefChanged();

  raw_ptr<PrefService> prefs_;
  base::RepeatingClosure local_changed_;
  PrefChangeRegistrar registrar_;
  bool applying_ = false;
  // The containers and the synced rules' hosts of the running sync's
  // snapshot. One missing now was deleted since, and isn't recreated.
  std::set<std::string> snapshot_containers_;
  std::set<std::string> snapshot_sites_;
};

}  // namespace firefox_sync

#endif  // CHROME_BROWSER_FIREFOX_SYNC_CONTAINERS_ADAPTER_H_
