// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "components/browser_traffic/browser_traffic.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_traffic {
namespace {

// The rows of a Windows per-user install; Linux has no browser updates row.
constexpr Row kRows[] = {Row::kCertificates,   Row::kDrm,
                         Row::kExtensions,     Row::kContentBlocker,
                         Row::kDictionaries,   Row::kBangs,
                         Row::kBrowserUpdates};

class BrowserTrafficTest : public testing::Test {
 protected:
  void SetUp() override { RegisterLocalStatePrefs(prefs_.registry()); }

  TestingPrefServiceSimple prefs_;
};

TEST_F(BrowserTrafficTest, DefaultIsDirect) {
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kDirect);
  EXPECT_EQ(GetRoute(prefs_, Row::kCertificates), Route::kDirect);
  // A row only has its own routes.
  EXPECT_EQ(GetRoute(prefs_, Row::kBangs), Route::kProxy);
  EXPECT_EQ(GetRoute(prefs_, Row::kBrowserUpdates), Route::kDirect);
  EXPECT_FALSE(NeedsServer(prefs_, kRows));
  EXPECT_FALSE(HasChosen(prefs_));
}

TEST_F(BrowserTrafficTest, PresetsAreDerived) {
  ApplyPreset(prefs_, Preset::kThroughServer, kRows);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kThroughServer);
  EXPECT_EQ(GetRoute(prefs_, Row::kExtensions), Route::kProxy);
  // Browser updates can't go through the server: the preset skips the route.
  EXPECT_EQ(GetRoute(prefs_, Row::kBrowserUpdates), Route::kDirect);
  EXPECT_EQ(GetHealthCheck(prefs_), HealthCheck::kPeriodically);
  EXPECT_TRUE(NeedsServer(prefs_, kRows));

  // Shut in leaves the routes as they are.
  ApplyPreset(prefs_, Preset::kShutIn, kRows);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kShutIn);
  EXPECT_EQ(GetRoute(prefs_, Row::kExtensions), Route::kProxy);
  EXPECT_EQ(GetDownload(prefs_, Row::kCertificates), Download::kOnRequest);
  EXPECT_EQ(GetHealthCheck(prefs_), HealthCheck::kOnRequest);

  // One row off a preset makes Custom.
  ApplyPreset(prefs_, Preset::kDirect, kRows);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kDirect);
  SetRoute(prefs_, Row::kDictionaries, Route::kProxy);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kCustom);
  SetRoute(prefs_, Row::kDictionaries, Route::kDirect);
  SetDownload(prefs_, Row::kBangs, Download::kOnRequest);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kCustom);

  // Custom sets nothing.
  ApplyPreset(prefs_, Preset::kCustom, kRows);
  EXPECT_EQ(GetDownload(prefs_, Row::kBangs), Download::kOnRequest);
}

TEST_F(BrowserTrafficTest, PresetsSetOnlyTheShownRows) {
  constexpr Row kLinuxRows[] = {Row::kCertificates, Row::kDrm,
                                Row::kExtensions,   Row::kContentBlocker,
                                Row::kDictionaries, Row::kBangs};
  ApplyPreset(prefs_, Preset::kShutIn, kLinuxRows);
  EXPECT_EQ(GetPreset(prefs_, kLinuxRows), Preset::kShutIn);
  EXPECT_EQ(GetDownload(prefs_, Row::kBrowserUpdates), Download::kAutomatic);
  EXPECT_EQ(GetPreset(prefs_, kRows), Preset::kCustom);
}

TEST_F(BrowserTrafficTest, ARouteTheRowLacksIsIgnored) {
  SetRoute(prefs_, Row::kBangs, Route::kDirect);
  SetRoute(prefs_, Row::kBrowserUpdates, Route::kProxy);
  EXPECT_EQ(GetRoute(prefs_, Row::kBangs), Route::kProxy);
  EXPECT_EQ(GetRoute(prefs_, Row::kBrowserUpdates), Route::kDirect);
}

TEST_F(BrowserTrafficTest, ServerMustBeHttpsOrLocalHttp) {
  EXPECT_EQ(ParseServer("https://services.example.org"),
            GURL("https://services.example.org/"));
  EXPECT_EQ(ParseServer("https://example.org/?x=1#y"),
            GURL("https://example.org/"));
  EXPECT_FALSE(ParseServer("https://example.org/helium"));
  EXPECT_EQ(ParseServer("http://localhost:8080"),
            GURL("http://localhost:8080/"));
  EXPECT_EQ(ParseServer("http://127.0.0.1"), GURL("http://127.0.0.1/"));
  EXPECT_FALSE(ParseServer(""));
  EXPECT_FALSE(ParseServer("services.example.org"));
  EXPECT_FALSE(ParseServer("http://services.example.org"));
  EXPECT_FALSE(ParseServer("ftp://localhost"));
  EXPECT_FALSE(ParseServer("file:///srv"));
  EXPECT_FALSE(ParseServer("https://"));
}

TEST_F(BrowserTrafficTest, ProxyURLsAreOnTheServer) {
  SetServer(prefs_, "https://example.org");
  SetRoute(prefs_, Row::kCertificates, Route::kProxy);
  SetRoute(prefs_, Row::kContentBlocker, Route::kProxy);
  EXPECT_EQ(GetProxyURL(prefs_, Row::kCertificates, "com"),
            GURL("https://example.org/com"));
  EXPECT_EQ(GetProxyURL(prefs_, Row::kContentBlocker, "ubo/assets.json"),
            GURL("https://example.org/ubo/assets.json"));
  EXPECT_EQ(GetProxyURL(prefs_, Row::kBangs, "bangs.json"),
            GURL("https://example.org/bangs.json"));
  // A direct row goes to its own source.
  EXPECT_FALSE(GetProxyURL(prefs_, Row::kExtensions, "ext"));
  EXPECT_FALSE(IsIdle(prefs_, Row::kExtensions));
}

// Without a valid server a proxied row has no URL at all: never direct, never
// Helium's own server.
TEST_F(BrowserTrafficTest, NoFallback) {
  ApplyPreset(prefs_, Preset::kThroughServer, kRows);
  for (const char* server : {"", "not a url", "http://example.org",
                             "https://services.helium.imput.net:bad"}) {
    SetServer(prefs_, server);
    for (Row row : kRows) {
      if (row == Row::kBrowserUpdates) {
        continue;
      }
      EXPECT_FALSE(GetProxyURL(prefs_, row, "x")) << server;
      EXPECT_TRUE(IsIdle(prefs_, row)) << server;
      EXPECT_FALSE(FetchesAutomatically(prefs_, row)) << server;
    }
    EXPECT_TRUE(FetchesAutomatically(prefs_, Row::kBrowserUpdates));
  }
  SetServer(prefs_, "https://services.example.org");
  EXPECT_TRUE(FetchesAutomatically(prefs_, Row::kCertificates));
}

TEST_F(BrowserTrafficTest, OnlyAutomaticCertificateDataTurnsStale) {
  const base::Time now = base::Time::Now();
  RecordCertificatesUpdated(prefs_, now);
  EXPECT_FALSE(CertificatesStale(prefs_, now + base::Hours(23)));
  EXPECT_EQ(CertificatesStaleAt(prefs_, now), now + kCertificatesStaleAfter);
  EXPECT_TRUE(CertificatesStale(prefs_, now + base::Hours(25)));
  EXPECT_FALSE(CertificatesStaleAt(prefs_, now + base::Hours(25)));

  SetDownload(prefs_, Row::kCertificates, Download::kOnRequest);
  EXPECT_FALSE(CertificatesStale(prefs_, now + base::Days(30)));
  EXPECT_FALSE(CertificatesStaleAt(prefs_, now));
}

TEST_F(BrowserTrafficTest, ComponentsHaveRows) {
  EXPECT_EQ(RowForComponent("hfnkpimlhhgieaddgfemjhofmfblmnib"),
            Row::kCertificates);
  EXPECT_EQ(RowForComponent("efniojlnjndmcbiieegkicadnoecjjef"),
            Row::kCertificates);
  EXPECT_EQ(RowForComponent("oimompecagnajdejgnnjijobebaeigek"), Row::kDrm);
  EXPECT_FALSE(RowForComponent("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  for (Row row : kAllRows) {
    EXPECT_EQ(RowFromName(RowName(row)), row);
  }
}

}  // namespace
}  // namespace browser_traffic
