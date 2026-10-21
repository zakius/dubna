// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef COMPONENTS_BROWSER_TRAFFIC_BROWSER_TRAFFIC_H_
#define COMPONENTS_BROWSER_TRAFFIC_BROWSER_TRAFFIC_H_

// Browser traffic: what the browser downloads by itself, and which way it
// goes. Each row, a kind of download, has a Download setting, Automatic or On
// request, and a Route, Direct to its source or Proxy through one server
// running helium-services (https://github.com/imputnet/helium-services), whose
// URL paths are the interface. A row only has the routes its source allows. A
// proxied row never falls back to its direct source: without a valid server it
// stays idle. The settings are in Local State, one set for each OS user.

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "url/gurl.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace browser_traffic {

enum class Row {
  kCertificates,     // CRLSet, the Chrome Root Store, the CT log list
  kDrm,              // Widevine
  kExtensions,       // store installs, item info, updates
  kContentBlocker,   // uBlock Origin's filter lists
  kDictionaries,     // spellcheck dictionaries
  kBangs,            // the !bang list: only through the server
  kBrowserUpdates,   // the browser's own updates: only direct
};
inline constexpr Row kAllRows[] = {
    Row::kCertificates, Row::kDrm,   Row::kExtensions,    Row::kContentBlocker,
    Row::kDictionaries, Row::kBangs, Row::kBrowserUpdates};

// The values are what the prefs store.
enum class Download { kAutomatic = 0, kOnRequest = 1 };
enum class Route { kDirect = 0, kProxy = 1 };
enum class HealthCheck { kPeriodically = 0, kOnRequest = 1 };

// Derived from the rows, never stored: Custom when no preset matches.
enum class Preset { kDirect, kThroughServer, kShutIn, kCustom };

// Certificate data older than this, while its row is Automatic, is stale.
inline constexpr base::TimeDelta kCertificatesStaleAfter = base::Hours(24);

// helium-services answers 204 here.
inline constexpr char kConnectivityCheckPath[] = "connectivitycheck";

// The row's name in its prefs and in the settings pages.
COMPONENT_EXPORT(BROWSER_TRAFFIC) std::string_view RowName(Row row);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<Row> RowFromName(std::string_view name);
COMPONENT_EXPORT(BROWSER_TRAFFIC) bool HasRoute(Row row, Route route);

// The row of a component updater component, by CRX id.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<Row> RowForComponent(std::string_view crx_id);
// The components a row updates.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
base::span<const std::string_view> ComponentsOf(Row row);

COMPONENT_EXPORT(BROWSER_TRAFFIC)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
// Adds the prefs to a Local State registrar: the settings' with one
// observer, the recorded results' with the other.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void AddObserver(PrefChangeRegistrar& registrar,
                 const base::RepeatingClosure& settings_observer,
                 const base::RepeatingClosure& results_observer);

// Local State, for code that only has a profile's prefs. The browser sets it
// once Local State is loaded and clears it at shutdown.
COMPONENT_EXPORT(BROWSER_TRAFFIC) void SetLocalState(PrefService* local_state);
COMPONENT_EXPORT(BROWSER_TRAFFIC) PrefService* GetLocalState();

COMPONENT_EXPORT(BROWSER_TRAFFIC)
Download GetDownload(const PrefService& local_state, Row row);
// A route the row has, whatever the pref holds.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
Route GetRoute(const PrefService& local_state, Row row);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void SetDownload(PrefService& local_state, Row row, Download download);
// Ignores a route the row doesn't have.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void SetRoute(PrefService& local_state, Row row, Route route);

// The server's origin, if `url` may be the server: https, or http on
// localhost, with no path, as helium-services' paths are absolute. Query and
// fragment are dropped.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<GURL> ParseServer(std::string_view url);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<GURL> GetServer(const PrefService& local_state);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void SetServer(PrefService& local_state, std::string_view url);

// `path` (relative, as "com" or "ubo/assets.json") on the server, when the
// row is proxied and the server is valid. Nothing otherwise: a direct row
// goes to its own source, and a proxied one without a server stays idle.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<GURL> GetProxyURL(const PrefService& local_state,
                                Row row,
                                std::string_view path);
// Proxied, with no valid server.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
bool IsIdle(const PrefService& local_state, Row row);
// Automatic and not idle: the browser may fetch without being asked.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
bool FetchesAutomatically(const PrefService& local_state, Row row);

// `rows` are the ones the browser shows; a preset sets only those.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
Preset GetPreset(const PrefService& local_state, base::span<const Row> rows);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void ApplyPreset(PrefService& local_state,
                 Preset preset,
                 base::span<const Row> rows);
// Some row that could go direct goes through the server, so the server is
// needed. A proxy-only row (bangs) doesn't need one: it stays off without.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
bool NeedsServer(const PrefService& local_state, base::span<const Row> rows);

COMPONENT_EXPORT(BROWSER_TRAFFIC)
HealthCheck GetHealthCheck(const PrefService& local_state);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void SetHealthCheck(PrefService& local_state, HealthCheck health_check);

// Whether the user chose, in setup or in Settings. Until then the defaults,
// the Direct preset, apply.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
bool HasChosen(const PrefService& local_state);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void SetChosen(PrefService& local_state);

// Results of the health check and of real fetches through the server.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void RecordServerResult(PrefService& local_state, bool answered, base::Time now);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
base::Time GetServerAnswered(const PrefService& local_state);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
base::Time GetServerFailed(const PrefService& local_state);

// The last time the certificate data was found current.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
void RecordCertificatesUpdated(PrefService& local_state, base::Time now);
COMPONENT_EXPORT(BROWSER_TRAFFIC)
base::Time GetCertificatesUpdated(const PrefService& local_state);
// Automatic certificate data more than kCertificatesStaleAfter old, for any
// reason. A row set to On request is never stale: its user updates it.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
bool CertificatesStale(const PrefService& local_state, base::Time now);
// When the data turns stale, if it isn't yet.
COMPONENT_EXPORT(BROWSER_TRAFFIC)
std::optional<base::Time> CertificatesStaleAt(const PrefService& local_state,
                                              base::Time now);

}  // namespace browser_traffic

#endif  // COMPONENTS_BROWSER_TRAFFIC_BROWSER_TRAFFIC_H_
