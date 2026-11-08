// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include <string_view>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "chrome/browser/containers/container.h"
#include "chrome/browser/containers/traffic_control.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace containers {
namespace {

base::DictValue ParseJsonDict(std::string_view json) {
  return *base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
}

base::ListValue ParseJsonList(std::string_view json) {
  return *base::JSONReader::ReadList(json, base::JSON_PARSE_RFC);
}

TEST(ContainersTest, ContainerFromDict) {
  std::optional<Container> container = Container::FromDict(
      ParseJsonDict(R"({"id": "work", "name": "Work",
                                    "icon": "briefcase", "color": "orange"})"));
  ASSERT_TRUE(container);
  EXPECT_EQ(container->id, "work");
  EXPECT_EQ(container->name, "Work");
  EXPECT_EQ(container->icon, "briefcase");
  EXPECT_EQ(container->color_value(), SkColorSetRGB(0xff, 0x9f, 0x00));
  EXPECT_FALSE(container->temporary());
  EXPECT_EQ(Container::FromDict(container->ToDict())->ToDict(),
            container->ToDict());
}

TEST(ContainersTest, InvalidContainers) {
  // No id, an id a partition name can't be, a name that isn't a string.
  for (std::string_view json :
       {R"({"name": "Work"})", R"({"id": "../x"})", R"({"id": ""})",
        R"({"id": "work", "name": 1})"}) {
    EXPECT_FALSE(Container::FromDict(ParseJsonDict(json))) << json;
  }
}

TEST(ContainersTest, ContainersFromList) {
  std::vector<Container> list = ContainersFromList(ParseJsonList(
      R"([{"id": "a"}, 5, {"id": "a", "name": "again"}, {"id": "t-1"}])"));
  ASSERT_EQ(list.size(), 2u);
  EXPECT_EQ(list[0].id, "a");
  EXPECT_EQ(list[0].name, "");
  EXPECT_TRUE(list[1].temporary());
  // An unknown color is gray.
  EXPECT_EQ(list[0].color_value(), SkColorSetRGB(0x7c, 0x7c, 0x7d));
}

TEST(ContainersTest, UrlFilters) {
  EXPECT_TRUE(IsValidUrlFilter("example.com"));
  EXPECT_TRUE(IsValidUrlFilter("# mail\nmail.example.com\n\nhttps://x.org/a"));
  EXPECT_FALSE(IsValidUrlFilter(""));
  EXPECT_FALSE(IsValidUrlFilter("# only a comment"));
  EXPECT_FALSE(IsValidUrlFilter("example.com\nhttps://"));
}

TEST(ContainersTest, TrafficRulesMatchFirstEnabledRule) {
  TrafficRules rules(ParseJsonList(R"([
      {"url_filter": "shop.example", "container": "a", "enabled": false},
      {"url_filter": "# comment\nexample.com", "container": "work"},
      {"url_filter": "mail.example.com", "container": "temporary"},
      {"url_filter": "shop.example", "container": ""},
      {"url_filter": "bad.example", "container": "../x"}])"));
  const TrafficRule* rule = rules.Match(GURL("https://mail.example.com/inbox"));
  ASSERT_TRUE(rule);
  EXPECT_EQ(rule->container, "work");
  rule = rules.Match(GURL("http://shop.example/"));
  ASSERT_TRUE(rule);
  EXPECT_EQ(rule->container, "");
  EXPECT_FALSE(rules.Match(GURL("https://bad.example/")));
  EXPECT_FALSE(rules.Match(GURL("https://other.org/")));
  EXPECT_FALSE(TrafficRules().Match(GURL("https://example.com/")));
}

TEST(ContainersTest, Reroute) {
  auto exists = [](std::string_view id) { return id != "gone"; };
  const TrafficRule work{.url_filter = "x", .container = "work"};
  const TrafficRule none{.url_filter = "x", .container = ""};
  const TrafficRule temporary{.url_filter = "x", .container = kTemporaryTarget};
  const TrafficRule gone{.url_filter = "x", .container = "gone"};

  EXPECT_EQ(Reroute(work, {.container = ""}, exists), "work");
  EXPECT_EQ(Reroute(work, {.container = "other"}, exists), "work");
  EXPECT_EQ(Reroute(work, {.container = "work"}, exists), std::nullopt);
  EXPECT_EQ(Reroute(none, {.container = "work"}, exists), "");
  EXPECT_EQ(Reroute(none, {.container = ""}, exists), std::nullopt);
  EXPECT_EQ(Reroute(temporary, {.container = "work"}, exists),
            kTemporaryTarget);
  EXPECT_EQ(Reroute(temporary, {.container = "t-1"}, exists), std::nullopt);
  // A rule whose container is gone moves nothing.
  EXPECT_EQ(Reroute(gone, {.container = ""}, exists), std::nullopt);

  // Within a site, a tab stays, unless it is new or the address was typed.
  EXPECT_EQ(Reroute(work, {.same_site = true}, exists), std::nullopt);
  EXPECT_EQ(Reroute(work, {.new_tab = true, .same_site = true}, exists),
            "work");
  EXPECT_EQ(Reroute(work, {.typed = true, .same_site = true}, exists), "work");
}

}  // namespace
}  // namespace containers

// Chromium's run_all_unittests needs its test fonts, which the browsers'
// trees don't download.
int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::i18n::InitializeICU();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
