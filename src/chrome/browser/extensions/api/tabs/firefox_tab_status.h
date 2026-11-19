// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_TAB_STATUS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_TAB_STATUS_H_

#include <optional>

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): tabs.onUpdated's status
// follows the tab's top frame, as in Firefox. Firefox sends "loading" when
// the top frame starts a navigation, before the URL, "loading" with the URL
// when it commits, and "complete" when the page stops loading, so a frame's
// navigation in the page sends nothing. TabsEventRouter sends "loading" when
// any frame commits a navigation, and "complete" when the page next stops
// loading. The router computes one update for every extension, so each of
// its status updates goes to an audience: Firefox add-ons, the other
// extensions, or all of them. The other extensions keep Chromium's updates.
class FirefoxTabStatus {
 public:
  enum class Audience {
    kAll,
    kFirefoxAddOns,
    kOtherExtensions,
  };

  // Whether an update for `audience` goes to `extension`, which is null for
  // a listener in a WebUI page.
  static bool Reaches(Audience audience, const Extension* extension);

  // Whether `navigation` sends Firefox add-ons "loading" before its URL: a
  // navigation of the tab's top frame to a new document.
  bool Started(content::NavigationHandle& navigation);

  // Whether a tab that the router starts to track sends Firefox add-ons
  // "loading": a new tab's first navigation starts before the tab is in a
  // window, so before the router sees it. The router asks in a task that it
  // posts when it starts to track the tab, so that the update comes after
  // tabs.onCreated and tabs.create()'s answer, as in Firefox.
  bool TrackedWhileLoading(content::WebContents& contents);

  // The audience of the router's status update for a committed navigation:
  // a frame's goes to the other extensions only.
  Audience Committed(bool is_main_frame);

  // The audience of "complete" when the tab stops loading, if any.
  // `router_waiting` is whether the router sends its own "complete", after
  // any frame's navigation; Firefox add-ons get theirs after a "loading" of
  // the top frame.
  std::optional<Audience> Stopped(bool router_waiting);

 private:
  // Whether Firefox add-ons had "loading" and wait for "complete".
  bool waiting_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_TAB_STATUS_H_
