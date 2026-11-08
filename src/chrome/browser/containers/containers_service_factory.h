// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace containers {

class ContainersService;

// Containers are for regular profiles: incognito and guest profiles have
// none.
class ContainersServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Null for a profile without containers.
  static ContainersService* GetForProfile(Profile* profile);
  static ContainersServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ContainersServiceFactory>;

  ContainersServiceFactory();
  ~ContainersServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINERS_SERVICE_FACTORY_H_
