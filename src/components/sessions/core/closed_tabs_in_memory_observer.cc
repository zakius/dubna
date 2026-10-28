// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "components/sessions/core/closed_tabs_in_memory_observer.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/closed_tabs_in_memory.h"
#include "components/sessions/core/tab_restore_service.h"

namespace sessions {

// static
void ClosedTabsInMemoryObserver::Attach(TabRestoreService* service,
                                        PrefService* prefs) {
  new ClosedTabsInMemoryObserver(service, prefs);
}

ClosedTabsInMemoryObserver::ClosedTabsInMemoryObserver(
    TabRestoreService* service,
    PrefService* prefs) {
  service->AddObserver(this);
  registrar_.Init(prefs);
  registrar_.Add(
      kKeepClosedTabsInMemory,
      base::BindRepeating(&ClosedTabsInMemoryObserver::TabRestoreServiceChanged,
                          base::Unretained(this), base::Unretained(service)));
}

ClosedTabsInMemoryObserver::~ClosedTabsInMemoryObserver() = default;

// The service's observer list allows reentrancy: ClearEntries() notifies
// again, and finds nothing left to clear.
void ClosedTabsInMemoryObserver::TabRestoreServiceChanged(
    TabRestoreService* service) {
  if (!KeepsClosedTabs(*registrar_.prefs()) && !service->entries().empty()) {
    service->ClearEntries();
  }
}

void ClosedTabsInMemoryObserver::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  service->RemoveObserver(this);
  delete this;
}

}  // namespace sessions
