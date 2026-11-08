// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
//
// Follows Brave's components/traffic_control/core/browser/traffic_rule_matcher.cc,
// rule_validation.cc and browser/traffic_control/traffic_control_apply.cc:
// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "chrome/browser/containers/traffic_control.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/containers/container.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace containers {

namespace {

constexpr char kUrlFilter[] = "url_filter";
constexpr char kContainer[] = "container";
constexpr char kEnabled[] = "enabled";

// The lines of `url_filter` that are patterns.
std::vector<std::string> Patterns(std::string_view url_filter) {
  std::vector<std::string> patterns;
  for (std::string_view line : base::SplitStringPiece(
           url_filter, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (!line.starts_with('#')) {
      patterns.emplace_back(line);
    }
  }
  return patterns;
}

}  // namespace

// static
std::optional<TrafficRule> TrafficRule::FromDict(const base::DictValue& dict) {
  const std::string* url_filter = dict.FindString(kUrlFilter);
  const std::string* container = dict.FindString(kContainer);
  if (!url_filter || !container) {
    return std::nullopt;
  }
  if (!container->empty() && *container != kTemporaryTarget &&
      !Container::IsValidId(*container)) {
    return std::nullopt;
  }
  return TrafficRule{.url_filter = *url_filter,
                     .container = *container,
                     .enabled = dict.FindBool(kEnabled).value_or(true)};
}

base::DictValue TrafficRule::ToDict() const {
  return base::DictValue()
      .Set(kUrlFilter, url_filter)
      .Set(kContainer, container)
      .Set(kEnabled, enabled);
}

bool IsValidUrlFilter(std::string_view url_filter) {
  std::vector<std::string> patterns = Patterns(url_filter);
  if (patterns.empty()) {
    return false;
  }
  for (const std::string& pattern : patterns) {
    url_matcher::util::FilterComponents components;
    if (!url_matcher::util::FilterToComponents(
            pattern, &components.scheme, &components.host,
            &components.match_subdomains, &components.port, &components.path,
            &components.query)) {
      return false;
    }
  }
  return true;
}

TrafficRules::TrafficRules() = default;

TrafficRules::TrafficRules(const base::ListValue& list)
    : matcher_(std::make_unique<url_matcher::URLMatcher>()) {
  url_matcher::URLMatcherConditionSet::Vector conditions;
  for (const base::Value& value : list) {
    std::optional<TrafficRule> rule =
        value.is_dict() ? TrafficRule::FromDict(value.GetDict()) : std::nullopt;
    if (!rule || !rule->enabled) {
      continue;
    }
    for (const std::string& pattern : Patterns(rule->url_filter)) {
      url_matcher::util::FilterComponents components;
      if (!url_matcher::util::FilterToComponents(
              pattern, &components.scheme, &components.host,
              &components.match_subdomains, &components.port, &components.path,
              &components.query)) {
        continue;
      }
      conditions.push_back(url_matcher::util::CreateConditionSet(
          matcher_.get(), rule_of_pattern_.size(), components.scheme,
          components.host, components.match_subdomains, components.port,
          components.path, components.query, /*allow=*/true));
      rule_of_pattern_.push_back(rules_.size());
    }
    rules_.push_back(std::move(*rule));
  }
  matcher_->AddConditionSets(conditions);
}

TrafficRules::TrafficRules(TrafficRules&&) = default;
TrafficRules& TrafficRules::operator=(TrafficRules&&) = default;
TrafficRules::~TrafficRules() = default;

const TrafficRule* TrafficRules::Match(const GURL& url) const {
  if (!matcher_ || !url.is_valid()) {
    return nullptr;
  }
  std::optional<size_t> first;
  for (base::MatcherStringPattern::ID pattern : matcher_->MatchURL(url)) {
    size_t rule = rule_of_pattern_[pattern];
    if (!first || rule < *first) {
      first = rule;
    }
  }
  return first ? &rules_[*first] : nullptr;
}

std::optional<std::string> Reroute(
    const TrafficRule& rule,
    const Navigation& navigation,
    base::FunctionRef<bool(std::string_view id)> exists) {
  if (rule.container == kTemporaryTarget) {
    if (navigation.container.starts_with(kTemporaryIdPrefix)) {
      return std::nullopt;
    }
  } else if (navigation.container == rule.container ||
             (!rule.container.empty() && !exists(rule.container))) {
    return std::nullopt;
  }
  if (navigation.same_site && !navigation.new_tab && !navigation.typed) {
    return std::nullopt;
  }
  return rule.container;
}

}  // namespace containers
