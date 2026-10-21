// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "components/browser_traffic/browser_traffic.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

namespace browser_traffic {

namespace {

constexpr char kServerPref[] = "browser_traffic.server";
constexpr char kHealthCheckPref[] = "browser_traffic.health_check";
constexpr char kChosenPref[] = "browser_traffic.chosen";
constexpr char kServerAnsweredPref[] = "browser_traffic.server_answered";
constexpr char kServerFailedPref[] = "browser_traffic.server_failed";
constexpr char kCertificatesUpdatedPref[] =
    "browser_traffic.certificates_updated";

PrefService* g_local_state = nullptr;

constexpr std::string_view kCertificateComponents[] = {
    "efniojlnjndmcbiieegkicadnoecjjef",  // PKI Metadata: root store, CT logs
    "hfnkpimlhhgieaddgfemjhofmfblmnib",  // CRLSet
};
constexpr std::string_view kDrmComponents[] = {
    "oimompecagnajdejgnnjijobebaeigek",  // Widevine
};

std::string DownloadPref(Row row) {
  return base::StrCat({"browser_traffic.", RowName(row), ".download"});
}

std::string RoutePref(Row row) {
  return base::StrCat({"browser_traffic.", RowName(row), ".route"});
}

// The route a row has when it has one only.
Route DefaultRoute(Row row) {
  return HasRoute(row, Route::kDirect) ? Route::kDirect : Route::kProxy;
}

// The download setting and the route a preset sets; no route leaves routes
// as they are.
struct PresetRows {
  Download download;
  std::optional<Route> route;
};

std::optional<PresetRows> RowsOf(Preset preset) {
  switch (preset) {
    case Preset::kDirect:
      return PresetRows{Download::kAutomatic, Route::kDirect};
    case Preset::kThroughServer:
      return PresetRows{Download::kAutomatic, Route::kProxy};
    case Preset::kShutIn:
      return PresetRows{Download::kOnRequest, std::nullopt};
    case Preset::kCustom:
      return std::nullopt;
  }
}

bool Matches(const PrefService& local_state,
             PresetRows preset,
             base::span<const Row> rows) {
  return std::ranges::all_of(rows, [&](Row row) {
    return GetDownload(local_state, row) == preset.download &&
           (!preset.route || !HasRoute(row, *preset.route) ||
            GetRoute(local_state, row) == *preset.route);
  });
}

}  // namespace

std::string_view RowName(Row row) {
  switch (row) {
    case Row::kCertificates:
      return "certificates";
    case Row::kDrm:
      return "drm";
    case Row::kExtensions:
      return "extensions";
    case Row::kContentBlocker:
      return "content_blocker";
    case Row::kDictionaries:
      return "dictionaries";
    case Row::kBangs:
      return "bangs";
    case Row::kBrowserUpdates:
      return "browser_updates";
  }
}

std::optional<Row> RowFromName(std::string_view name) {
  for (Row row : kAllRows) {
    if (RowName(row) == name) {
      return row;
    }
  }
  return std::nullopt;
}

bool HasRoute(Row row, Route route) {
  switch (row) {
    case Row::kBangs:
      return route == Route::kProxy;
    case Row::kBrowserUpdates:
      return route == Route::kDirect;
    default:
      return true;
  }
}

std::optional<Row> RowForComponent(std::string_view crx_id) {
  for (Row row : {Row::kCertificates, Row::kDrm}) {
    if (std::ranges::find(ComponentsOf(row), crx_id) !=
        ComponentsOf(row).end()) {
      return row;
    }
  }
  return std::nullopt;
}

base::span<const std::string_view> ComponentsOf(Row row) {
  switch (row) {
    case Row::kCertificates:
      return kCertificateComponents;
    case Row::kDrm:
      return kDrmComponents;
    default:
      return {};
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  for (Row row : kAllRows) {
    registry->RegisterIntegerPref(DownloadPref(row),
                                  static_cast<int>(Download::kAutomatic));
    registry->RegisterIntegerPref(RoutePref(row),
                                  static_cast<int>(DefaultRoute(row)));
  }
  registry->RegisterStringPref(kServerPref, std::string());
  registry->RegisterIntegerPref(kHealthCheckPref,
                                static_cast<int>(HealthCheck::kPeriodically));
  registry->RegisterBooleanPref(kChosenPref, false);
  registry->RegisterTimePref(kServerAnsweredPref, base::Time());
  registry->RegisterTimePref(kServerFailedPref, base::Time());
  registry->RegisterTimePref(kCertificatesUpdatedPref, base::Time());
}

void AddObserver(PrefChangeRegistrar& registrar,
                 const base::RepeatingClosure& settings_observer,
                 const base::RepeatingClosure& results_observer) {
  for (Row row : kAllRows) {
    registrar.Add(DownloadPref(row), settings_observer);
    registrar.Add(RoutePref(row), settings_observer);
  }
  registrar.Add(kServerPref, settings_observer);
  registrar.Add(kHealthCheckPref, settings_observer);
  for (const char* pref : {kServerAnsweredPref, kServerFailedPref,
                           kCertificatesUpdatedPref}) {
    registrar.Add(pref, results_observer);
  }
}

void SetLocalState(PrefService* local_state) {
  g_local_state = local_state;
  // The first run starts the clock of the certificate data, which the build
  // brings with it.
  if (local_state && GetCertificatesUpdated(*local_state).is_null()) {
    RecordCertificatesUpdated(*local_state, base::Time::Now());
  }
}

PrefService* GetLocalState() {
  return g_local_state;
}

Download GetDownload(const PrefService& local_state, Row row) {
  return local_state.GetInteger(DownloadPref(row)) ==
                 static_cast<int>(Download::kOnRequest)
             ? Download::kOnRequest
             : Download::kAutomatic;
}

Route GetRoute(const PrefService& local_state, Row row) {
  const Route route = local_state.GetInteger(RoutePref(row)) ==
                              static_cast<int>(Route::kProxy)
                          ? Route::kProxy
                          : Route::kDirect;
  return HasRoute(row, route) ? route : DefaultRoute(row);
}

void SetDownload(PrefService& local_state, Row row, Download download) {
  local_state.SetInteger(DownloadPref(row), static_cast<int>(download));
}

void SetRoute(PrefService& local_state, Row row, Route route) {
  if (HasRoute(row, route)) {
    local_state.SetInteger(RoutePref(row), static_cast<int>(route));
  }
}

std::optional<GURL> ParseServer(std::string_view url) {
  const GURL server(url);
  if (!server.is_valid() || !server.has_host() || server.path() != "/" ||
      !(server.SchemeIs(url::kHttpsScheme) ||
        (server.SchemeIs(url::kHttpScheme) && net::IsLocalhost(server)))) {
    return std::nullopt;
  }
  return server.GetWithEmptyPath();
}

std::optional<GURL> GetServer(const PrefService& local_state) {
  return ParseServer(local_state.GetString(kServerPref));
}

void SetServer(PrefService& local_state, std::string_view url) {
  local_state.SetString(kServerPref, std::string(url));
}

std::optional<GURL> GetProxyURL(const PrefService& local_state,
                                Row row,
                                std::string_view path) {
  DCHECK(!path.starts_with('/'));
  if (GetRoute(local_state, row) != Route::kProxy) {
    return std::nullopt;
  }
  std::optional<GURL> server = GetServer(local_state);
  if (!server) {
    return std::nullopt;
  }
  return server->Resolve(path);
}

bool IsIdle(const PrefService& local_state, Row row) {
  return GetRoute(local_state, row) == Route::kProxy &&
         !GetServer(local_state);
}

bool FetchesAutomatically(const PrefService& local_state, Row row) {
  return GetDownload(local_state, row) == Download::kAutomatic &&
         !IsIdle(local_state, row);
}

Preset GetPreset(const PrefService& local_state, base::span<const Row> rows) {
  for (Preset preset :
       {Preset::kDirect, Preset::kThroughServer, Preset::kShutIn}) {
    if (Matches(local_state, *RowsOf(preset), rows)) {
      return preset;
    }
  }
  return Preset::kCustom;
}

void ApplyPreset(PrefService& local_state,
                 Preset preset,
                 base::span<const Row> rows) {
  const std::optional<PresetRows> preset_rows = RowsOf(preset);
  if (!preset_rows) {
    return;
  }
  for (Row row : rows) {
    SetDownload(local_state, row, preset_rows->download);
    if (preset_rows->route) {
      SetRoute(local_state, row, *preset_rows->route);
    }
  }
  if (preset == Preset::kThroughServer) {
    SetHealthCheck(local_state, HealthCheck::kPeriodically);
  } else if (preset == Preset::kShutIn) {
    SetHealthCheck(local_state, HealthCheck::kOnRequest);
  }
}

bool NeedsServer(const PrefService& local_state, base::span<const Row> rows) {
  return std::ranges::any_of(rows, [&](Row row) {
    return HasRoute(row, Route::kDirect) &&
           GetRoute(local_state, row) == Route::kProxy;
  });
}

HealthCheck GetHealthCheck(const PrefService& local_state) {
  return local_state.GetInteger(kHealthCheckPref) ==
                 static_cast<int>(HealthCheck::kOnRequest)
             ? HealthCheck::kOnRequest
             : HealthCheck::kPeriodically;
}

void SetHealthCheck(PrefService& local_state, HealthCheck health_check) {
  local_state.SetInteger(kHealthCheckPref, static_cast<int>(health_check));
}

bool HasChosen(const PrefService& local_state) {
  return local_state.GetBoolean(kChosenPref);
}

void SetChosen(PrefService& local_state) {
  local_state.SetBoolean(kChosenPref, true);
}

void RecordServerResult(PrefService& local_state,
                        bool answered,
                        base::Time now) {
  local_state.SetTime(answered ? kServerAnsweredPref : kServerFailedPref, now);
}

base::Time GetServerAnswered(const PrefService& local_state) {
  return local_state.GetTime(kServerAnsweredPref);
}

base::Time GetServerFailed(const PrefService& local_state) {
  return local_state.GetTime(kServerFailedPref);
}

void RecordCertificatesUpdated(PrefService& local_state, base::Time now) {
  local_state.SetTime(kCertificatesUpdatedPref, now);
}

base::Time GetCertificatesUpdated(const PrefService& local_state) {
  return local_state.GetTime(kCertificatesUpdatedPref);
}

bool CertificatesStale(const PrefService& local_state, base::Time now) {
  return GetDownload(local_state, Row::kCertificates) ==
             Download::kAutomatic &&
         now - GetCertificatesUpdated(local_state) > kCertificatesStaleAfter;
}

std::optional<base::Time> CertificatesStaleAt(const PrefService& local_state,
                                              base::Time now) {
  if (GetDownload(local_state, Row::kCertificates) != Download::kAutomatic ||
      CertificatesStale(local_state, now)) {
    return std::nullopt;
  }
  return GetCertificatesUpdated(local_state) + kCertificatesStaleAfter;
}

}  // namespace browser_traffic
