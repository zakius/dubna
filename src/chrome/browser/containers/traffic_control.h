// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
//
// Traffic Control's rules and decision follow Brave's
// components/traffic_control and browser/traffic_control:
// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_H_
#define CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/values.h"

class GURL;

namespace url_matcher {
class URLMatcher;
}

namespace containers {

// A rule's container that is a new temporary container each time.
inline constexpr char kTemporaryTarget[] = "temporary";

// Traffic Control: pages whose address matches a rule open in the rule's
// container, whichever tab they start in.
struct TrafficRule {
  // nullopt for a dict with a value of the wrong type.
  static std::optional<TrafficRule> FromDict(const base::DictValue& dict);

  base::DictValue ToDict() const;

  // Patterns of the URL blocklist policy's format ("example.com",
  // "https://*.example.org/path"), one per line. Blank lines and lines that
  // start with # don't count.
  std::string url_filter;
  // A container's id, kTemporaryTarget, or empty: outside containers.
  std::string container;
  bool enabled = true;
};

// Whether `url_filter` has at least one pattern, and every pattern is valid.
bool IsValidUrlFilter(std::string_view url_filter);

// The rules of a list pref, in order.
class TrafficRules {
 public:
  TrafficRules();
  explicit TrafficRules(const base::ListValue& list);
  TrafficRules(TrafficRules&&);
  TrafficRules& operator=(TrafficRules&&);
  ~TrafficRules();

  // The first enabled rule with a pattern that `url` matches.
  const TrafficRule* Match(const GURL& url) const;

 private:
  std::vector<TrafficRule> rules_;
  // By pattern id, the index of its rule.
  std::vector<size_t> rule_of_pattern_;
  std::unique_ptr<url_matcher::URLMatcher> matcher_;
};

// What Traffic Control knows of a main frame navigation to a page `rule`
// matches.
struct Navigation {
  // The tab's container's id, or empty.
  std::string container;
  // The tab has no page yet, and no page holds it as window.opener.
  bool new_tab = false;
  // Typed in the address bar, or picked from its suggestions.
  bool typed = false;
  // The page it leaves is of the same site.
  bool same_site = false;
};

// The container that `navigation` goes to instead of its tab, under `rule`:
// a container's id, kTemporaryTarget, or empty, outside containers. nullopt
// keeps it in its tab: the tab is in the rule's container already, the rule's
// container is gone (`exists` says), or the tab browses within a site, which
// keeps a page the user opened elsewhere on purpose there. A temporary
// container's tab stays for a rule that wants a temporary container.
std::optional<std::string> Reroute(
    const TrafficRule& rule,
    const Navigation& navigation,
    base::FunctionRef<bool(std::string_view id)> exists);

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_H_
