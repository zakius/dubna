// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/containers/containers_service_factory.h"

#include "chrome/browser/containers/containers_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"

namespace containers {

// static
ContainersService* ContainersServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ContainersService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContainersServiceFactory* ContainersServiceFactory::GetInstance() {
  static base::NoDestructor<ContainersServiceFactory> instance;
  return instance.get();
}

ContainersServiceFactory::ContainersServiceFactory()
    : ProfileKeyedServiceFactory("ContainersService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(TabRestoreServiceFactory::GetInstance());
}

ContainersServiceFactory::~ContainersServiceFactory() = default;

std::unique_ptr<KeyedService>
ContainersServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ContainersService>(
      Profile::FromBrowserContext(context));
}

bool ContainersServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

void ContainersServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ContainersService::RegisterProfilePrefs(registry);
}

}  // namespace containers
