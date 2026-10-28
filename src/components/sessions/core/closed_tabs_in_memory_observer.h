// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef COMPONENTS_SESSIONS_CORE_CLOSED_TABS_IN_MEMORY_OBSERVER_H_
#define COMPONENTS_SESSIONS_CORE_CLOSED_TABS_IN_MEMORY_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/closed_tabs_in_memory.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class PrefService;

namespace sessions {

class TabRestoreService;

// Empties a TabRestoreService as soon as it holds closed windows, groups or
// splits that aren't to be kept (history isn't saved, nor kept in memory),
// and when keeping them in memory is turned off. Closed tabs never get in:
// TabRestoreServiceImpl::CreateHistoricalTab drops them first. Owns itself
// and goes away with the service.
class ClosedTabsInMemoryObserver : public TabRestoreServiceObserver {
 public:
  static void Attach(TabRestoreService* service, PrefService* prefs);

  ClosedTabsInMemoryObserver(const ClosedTabsInMemoryObserver&) = delete;
  ClosedTabsInMemoryObserver& operator=(const ClosedTabsInMemoryObserver&) =
      delete;

  // TabRestoreServiceObserver:
  void TabRestoreServiceChanged(TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(TabRestoreService* service) override;

 private:
  ClosedTabsInMemoryObserver(TabRestoreService* service, PrefService* prefs);
  ~ClosedTabsInMemoryObserver() override;

  PrefChangeRegistrar registrar_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_CLOSED_TABS_IN_MEMORY_OBSERVER_H_
