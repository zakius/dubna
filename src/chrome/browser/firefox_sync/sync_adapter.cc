// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/firefox_sync/sync_adapter.h"

#include <utility>

namespace firefox_sync {

void SyncAdapter::WhenApplied(base::OnceClosure done) {
  std::move(done).Run();
}

}  // namespace firefox_sync
