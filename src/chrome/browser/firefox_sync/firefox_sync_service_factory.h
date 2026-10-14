// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace firefox_sync {

class FirefoxSyncService;

// One FirefoxSyncService per regular profile, created with the profile so
// scheduled syncs run without opening settings.
class FirefoxSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FirefoxSyncService* GetForProfile(Profile* profile);
  static FirefoxSyncServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<FirefoxSyncServiceFactory>;

  FirefoxSyncServiceFactory();
  ~FirefoxSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace firefox_sync

#endif  // CHROME_BROWSER_FIREFOX_SYNC_FIREFOX_SYNC_SERVICE_FACTORY_H_
