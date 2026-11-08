// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
//
// Follows Brave's browser/traffic_control/traffic_control_navigation_throttle.cc:
// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "chrome/browser/containers/traffic_control_throttle.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/containers/containers_platform.h"
#include "chrome/browser/containers/containers_service.h"
#include "chrome/browser/containers/containers_service_factory.h"
#include "chrome/browser/containers/containers_tabs.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace containers {

namespace {

class TrafficControlThrottle : public content::NavigationThrottle {
 public:
  TrafficControlThrottle(content::NavigationThrottleRegistry& registry,
                         ContainersService& service)
      : content::NavigationThrottle(registry), service_(service) {}

  ThrottleCheckResult WillStartRequest() override { return Check(); }
  ThrottleCheckResult WillRedirectRequest() override { return Check(); }
  const char* GetNameForLogging() override { return "TrafficControlThrottle"; }

 private:
  ThrottleCheckResult Check() {
    content::NavigationHandle* handle = navigation_handle();
    // A page that loads again in its tab stays there: a reload, back and
    // forward, a restored tab.
    if (handle->IsSameDocument() || handle->IsDownload() ||
        handle->IsHistory() ||
        handle->GetReloadType() != content::ReloadType::NONE ||
        handle->GetRestoreType() != content::RestoreType::kNotRestored) {
      return PROCEED;
    }
    const TrafficRule* rule = service_->traffic_rules().Match(handle->GetURL());
    if (!rule) {
      return PROCEED;
    }
    content::WebContents* contents = handle->GetWebContents();
    content::SiteInstance* starting = handle->GetStartingSiteInstance();
    const ui::PageTransition transition = handle->GetPageTransition();
    Navigation navigation{
        .container = ContainerIdOf(contents),
        // Moving a tab that a page holds as window.opener would cut it off.
        .new_tab = !contents->HasOpener() &&
                   contents->GetController().IsInitialBlankNavigation(),
        .typed = ui::PageTransitionCoreTypeIs(transition,
                                              ui::PAGE_TRANSITION_TYPED) ||
                 (transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        .same_site = starting && starting->IsSameSiteWithURL(handle->GetURL()),
    };
    std::optional<std::string> target =
        Reroute(*rule, navigation, [this](std::string_view id) {
          // One of the user's: a deleted container doesn't come back.
          for (const Container& container : service_->GetContainers()) {
            if (container.id == id) {
              return true;
            }
          }
          return false;
        });
    if (!target) {
      return PROCEED;
    }
    if (*target == kTemporaryTarget) {
      *target = service_->CreateTemporary().id;
    }
    // After the navigation has ended, which may close the tab.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<content::WebContents> contents, GURL url,
               std::string container, ui::PageTransition transition,
               std::optional<url::Origin> initiator) {
              if (contents) {
                ReopenNavigation(contents.get(), url, container, transition,
                                 initiator);
              }
            },
            contents->GetWeakPtr(), handle->GetURL(), *target, transition,
            handle->GetInitiatorOrigin()));
    return CANCEL;
  }

  const raw_ref<ContainersService> service_;
};

}  // namespace

void MaybeAddTrafficControlThrottle(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }
  ContainersService* service =
      ContainersServiceFactory::GetForProfile(Profile::FromBrowserContext(
          handle.GetWebContents()->GetBrowserContext()));
  if (service) {
    registry.AddThrottle(
        std::make_unique<TrafficControlThrottle>(registry, *service));
  }
}

}  // namespace containers
