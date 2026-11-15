// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/firefox_sync/containers_adapter.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/containers/container.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace firefox_sync {
namespace {

using containers::kContainersList;
using containers::kContainersSyncDeleted;
using containers::kTrafficControlRules;

base::ListValue ParseJsonList(std::string_view json) {
  return *base::JSONReader::ReadList(json, base::JSON_PARSE_RFC);
}

class ContainersAdapterTest : public testing::Test {
 protected:
  // In memory. TestingPrefServiceSimple's target needs Chromium's test fonts,
  // which the browsers' trees don't download.
  ContainersAdapterTest() {
    auto registry = base::MakeRefCounted<PrefRegistrySimple>();
    for (const char* pref :
         {kContainersList, kTrafficControlRules, kContainersSyncDeleted}) {
      registry->RegisterListPref(pref);
    }
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<InMemoryPrefStore>());
    prefs_ = factory.Create(std::move(registry));
    prefs_->SetList(kContainersList, ParseJsonList(R"([
        {"id": "work", "name": "Work", "icon": "briefcase", "color": "orange"},
        {"id": "shop", "name": "Shop", "icon": "cart", "color": "pink"}])"));
    prefs_->SetList(kTrafficControlRules, ParseJsonList(R"([
        {"url_filter": ".example.com", "container": "work", "enabled": true},
        {"url_filter": "example.net", "container": "work", "enabled": true}])"));
    adapter_ = std::make_unique<ContainersAdapter>(
        prefs_.get(),
        base::BindRepeating([](int* count) { ++*count; }, &local_changes_));
  }

  // Starts a sync: the adapter takes the snapshot the ops are against.
  void GetLocal() { adapter_->GetLocal(base::DoNothing()); }

  void Apply(std::string_view ops) { adapter_->Apply(ParseJsonList(ops)); }

  const base::ListValue& Pref(const char* pref) {
    return prefs_->GetList(pref);
  }

  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<ContainersAdapter> adapter_;
  int local_changes_ = 0;
};

TEST_F(ContainersAdapterTest, Containers) {
  GetLocal();
  Apply(R"([
      {"ext": "@testpilot-containers", "op": "container", "id": "work",
       "name": "Office", "icon": "briefcase", "color": "blue"},
      {"ext": "@testpilot-containers", "op": "container", "id": "home",
       "name": "Home", "icon": "tree"},
      {"ext": "@testpilot-containers", "op": "delete_container",
       "id": "shop"},
      {"ext": "@testpilot-containers", "op": "delete_container",
       "id": "gone"}])");
  EXPECT_EQ(Pref(kContainersList), ParseJsonList(R"([
      {"id": "work", "name": "Office", "icon": "briefcase", "color": "blue"},
      {"id": "home", "name": "Home", "icon": "tree", "color": ""}])"));
  // The service asks the user before the deleted container's tabs and
  // storage go; a container that isn't here has neither.
  EXPECT_EQ(Pref(kContainersSyncDeleted), ParseJsonList(R"(["shop"])"));
  // The rules of a deleted container stay.
  EXPECT_EQ(Pref(kTrafficControlRules).size(), 2u);
  EXPECT_EQ(local_changes_, 0);
}

TEST_F(ContainersAdapterTest, Sites) {
  GetLocal();
  Apply(R"([
      {"ext": "@testpilot-containers", "op": "site", "host": "example.com",
       "container": "shop"},
      {"ext": "@testpilot-containers", "op": "site", "host": "example.org",
       "container": "work"},
      {"ext": "@testpilot-containers", "op": "delete_site",
       "host": "example.net"}])");
  // example.net's rule isn't a site assignment (no leading dot), so the
  // add-on's delete leaves it.
  EXPECT_EQ(Pref(kTrafficControlRules), ParseJsonList(R"([
      {"url_filter": ".example.com", "container": "shop", "enabled": true},
      {"url_filter": "example.net", "container": "work", "enabled": true},
      {"url_filter": ".example.org", "container": "work", "enabled": true}])"));

  GetLocal();
  Apply(R"([{"ext": "@testpilot-containers", "op": "delete_site",
             "host": "example.com"}])");
  EXPECT_EQ(Pref(kTrafficControlRules), ParseJsonList(R"([
      {"url_filter": "example.net", "container": "work", "enabled": true},
      {"url_filter": ".example.org", "container": "work", "enabled": true}])"));
  EXPECT_EQ(local_changes_, 0);
}

TEST_F(ContainersAdapterTest, DeletedHereDuringTheSync) {
  GetLocal();
  // The user deletes a container and a rule while the sync runs.
  prefs_->SetList(kContainersList, ParseJsonList(R"([
      {"id": "work", "name": "Work", "icon": "briefcase", "color": "orange"}])"));
  prefs_->SetList(kTrafficControlRules, ParseJsonList(R"([
      {"url_filter": "example.net", "container": "work", "enabled": true}])"));
  EXPECT_EQ(local_changes_, 2);
  base::ListValue list = Pref(kContainersList).Clone();
  base::ListValue rules = Pref(kTrafficControlRules).Clone();

  Apply(R"([
      {"ext": "@testpilot-containers", "op": "container", "id": "shop",
       "name": "Shop", "icon": "cart", "color": "pink"},
      {"ext": "@testpilot-containers", "op": "site", "host": "example.com",
       "container": "work"}])");
  EXPECT_EQ(Pref(kContainersList), list);
  EXPECT_EQ(Pref(kTrafficControlRules), rules);
  EXPECT_EQ(local_changes_, 2);
}

TEST_F(ContainersAdapterTest, OtherOpsIgnored) {
  GetLocal();
  base::ListValue list = Pref(kContainersList).Clone();
  base::ListValue rules = Pref(kTrafficControlRules).Clone();
  Apply(R"([
      {"ext": "other@example.com", "op": "container", "id": "home"},
      {"op": "container", "id": "home"},
      {"ext": "@testpilot-containers", "id": "home"},
      {"ext": "@testpilot-containers", "op": "container", "id": "not valid"},
      {"ext": "@testpilot-containers", "op": "container"},
      {"ext": "@testpilot-containers", "op": "site", "host": "example.org"},
      {"ext": "@testpilot-containers", "op": "rename", "id": "work"},
      "container"])");
  EXPECT_EQ(Pref(kContainersList), list);
  EXPECT_EQ(Pref(kTrafficControlRules), rules);
  EXPECT_TRUE(Pref(kContainersSyncDeleted).empty());
  EXPECT_EQ(local_changes_, 0);
}

}  // namespace
}  // namespace firefox_sync
