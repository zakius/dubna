// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/containers/containers_cleanup.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/containers/container.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace containers {
namespace {

// The browser's tabs, closed tabs and storage, as the cleanup sees them.
class FakeBrowser : public ContainersCleanup::Delegate {
 public:
  bool Keeps(const std::string& id) override { return listed.contains(id); }

  void LoadClosedTabs(base::OnceClosure loaded) override {
    std::move(loaded).Run();
  }

  void GetInUse(
      const std::set<std::string>& deleted,
      base::OnceCallback<void(std::set<std::string>)> callback) override {
    std::erase_if(closed_tabs,
                  [&](const std::string& id) { return deleted.contains(id); });
    // The tabs are counted now, the answer comes when `answer` runs.
    std::set<std::string> in_use = open_tabs;
    in_use.insert(closed_tabs.begin(), closed_tabs.end());
    answer = base::BindOnce(std::move(callback), std::move(in_use));
    if (!hold_answer) {
      std::move(answer).Run();
    }
  }

  void DeleteStorage(const std::string& id,
                     base::OnceCallback<void(bool)> done) override {
    deleted_storage.push_back(id);
    std::move(done).Run(storage_gone);
  }

  std::set<std::string> listed;
  std::set<std::string> open_tabs;
  std::set<std::string> closed_tabs;
  bool hold_answer = false;
  base::OnceClosure answer;
  bool storage_gone = true;
  std::vector<std::string> deleted_storage;
};

class ContainersCleanupTest : public testing::Test {
 protected:
  // In memory. TestingPrefServiceSimple's target needs Chromium's test fonts,
  // which the browsers' trees don't download.
  ContainersCleanupTest() {
    auto registry = base::MakeRefCounted<PrefRegistrySimple>();
    registry->RegisterDictionaryPref(kContainersUsed);
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<InMemoryPrefStore>());
    prefs_ = factory.Create(std::move(registry));
    cleanup_ = std::make_unique<ContainersCleanup>(prefs_.get(), &browser_);
  }

  // Container `id` has storage.
  void Use(const std::string& id) {
    ScopedDictPrefUpdate(prefs_.get(), kContainersUsed)
        ->Set(id, base::DictValue().Set("name", id));
  }

  std::set<std::string> Used() {
    std::set<std::string> used;
    for (auto [id, value] : prefs_->GetDict(kContainersUsed)) {
      used.insert(id);
    }
    return used;
  }

  // Runs the cleanup; the returned flag says whether it has finished.
  std::unique_ptr<bool> Run() {
    auto done = std::make_unique<bool>(false);
    cleanup_->Run(base::BindOnce([](bool* done) { *done = true; }, done.get()));
    return done;
  }

  FakeBrowser browser_;
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<ContainersCleanup> cleanup_;
};

TEST_F(ContainersCleanupTest, NothingToDelete) {
  browser_.listed = {"work"};
  Use("work");
  browser_.hold_answer = true;
  // Done at once, without asking the browser.
  EXPECT_TRUE(*Run());
  EXPECT_FALSE(browser_.answer);
  EXPECT_EQ(Used(), std::set<std::string>{"work"});
}

TEST_F(ContainersCleanupTest, KeepsWhatIsInUse) {
  browser_.listed = {"work"};
  Use("work");
  Use("t-open");
  Use("t-closed");
  Use("t-unused");
  browser_.open_tabs = {"t-open"};
  // A temporary container's closed tab keeps it, with its storage.
  browser_.closed_tabs = {"t-closed"};
  EXPECT_TRUE(*Run());
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"t-unused"});
  EXPECT_EQ(browser_.closed_tabs, std::set<std::string>{"t-closed"});
  EXPECT_EQ(Used(), (std::set<std::string>{"work", "t-open", "t-closed"}));
}

TEST_F(ContainersCleanupTest, DeletedContainerWithAClosedTab) {
  Use("deleted");
  browser_.closed_tabs = {"deleted"};
  EXPECT_TRUE(*Run());
  // Its closed tab goes, and with it what kept the container.
  EXPECT_TRUE(browser_.closed_tabs.empty());
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"deleted"});
  EXPECT_TRUE(Used().empty());
}

TEST_F(ContainersCleanupTest, TemporaryMadeWhileTheBrowserAnswers) {
  Use("deleted");
  browser_.hold_answer = true;
  std::unique_ptr<bool> done = Run();
  // Its tab opened after the browser counted the tabs.
  Use("t-new");
  std::move(browser_.answer).Run();
  EXPECT_TRUE(*done);
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"deleted"});
  EXPECT_EQ(Used(), std::set<std::string>{"t-new"});
}

TEST_F(ContainersCleanupTest, ListedAgainWhileTheBrowserAnswers) {
  Use("back");
  browser_.hold_answer = true;
  std::unique_ptr<bool> done = Run();
  // As when sync brings it back.
  browser_.listed = {"back"};
  std::move(browser_.answer).Run();
  EXPECT_TRUE(*done);
  EXPECT_TRUE(browser_.deleted_storage.empty());
  EXPECT_EQ(Used(), std::set<std::string>{"back"});
}

TEST_F(ContainersCleanupTest, ClearedStorageStaysUsed) {
  // A partition loaded in this run is only cleared: its folder goes at the
  // next start, so the container stays in the list of used ones.
  Use("loaded");
  browser_.storage_gone = false;
  EXPECT_TRUE(*Run());
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"loaded"});
  EXPECT_EQ(Used(), std::set<std::string>{"loaded"});
}

TEST_F(ContainersCleanupTest, RunDuringARun) {
  Use("first");
  browser_.hold_answer = true;
  std::unique_ptr<bool> first = Run();
  browser_.open_tabs = {"t-later"};
  Use("t-later");
  std::unique_ptr<bool> second = Run();
  // The second run starts once the first has finished.
  std::move(browser_.answer).Run();
  EXPECT_FALSE(*first);
  EXPECT_FALSE(*second);
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"first"});
  ASSERT_TRUE(browser_.answer);
  std::move(browser_.answer).Run();
  EXPECT_EQ(browser_.deleted_storage, std::vector<std::string>{"first"});
  EXPECT_TRUE(*first);
  EXPECT_TRUE(*second);
  EXPECT_EQ(Used(), std::set<std::string>{"t-later"});
}

TEST_F(ContainersCleanupTest, StopDropsTheAnswer) {
  Use("deleted");
  browser_.hold_answer = true;
  std::unique_ptr<bool> done = Run();
  cleanup_->Stop();
  std::move(browser_.answer).Run();
  EXPECT_FALSE(*done);
  EXPECT_TRUE(browser_.deleted_storage.empty());
  EXPECT_EQ(Used(), std::set<std::string>{"deleted"});
}

}  // namespace
}  // namespace containers
