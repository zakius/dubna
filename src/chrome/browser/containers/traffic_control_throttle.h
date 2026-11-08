// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_THROTTLE_H_
#define CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_THROTTLE_H_

namespace content {
class NavigationThrottleRegistry;
}

namespace containers {

// Adds Traffic Control's throttle to a main frame navigation of a profile
// with rules: one that a rule sends to another container stops, and its page
// opens in a new tab there (ReopenNavigation()).
void MaybeAddTrafficControlThrottle(
    content::NavigationThrottleRegistry& registry);

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_TRAFFIC_CONTROL_THROTTLE_H_
