// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_IDENTITIES_CONTEXTUAL_IDENTITIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_IDENTITIES_CONTEXTUAL_IDENTITIES_API_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/containers/container.h"
#include "chrome/browser/containers/containers_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// contextualIdentities API over the user's containers, read only. Fires
// onCreated, onUpdated and onRemoved when the user's list changes.
class ContextualIdentitiesAPI : public BrowserContextKeyedAPI,
                                public containers::ContainersService::Observer {
 public:
  explicit ContextualIdentitiesAPI(content::BrowserContext* context);
  ContextualIdentitiesAPI(const ContextualIdentitiesAPI&) = delete;
  ContextualIdentitiesAPI& operator=(const ContextualIdentitiesAPI&) = delete;
  ~ContextualIdentitiesAPI() override;

  static BrowserContextKeyedAPIFactory<ContextualIdentitiesAPI>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<ContextualIdentitiesAPI>;

  static const char* service_name() { return "ContextualIdentitiesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // containers::ContainersService::Observer:
  void OnContainersChanged() override;

  void Dispatch(const char* event_name, const containers::Container& container);

  raw_ptr<Profile> profile_;
  base::ScopedObservation<containers::ContainersService,
                          containers::ContainersService::Observer>
      observation_{this};
  // The user's list at the last change, to tell what changed.
  std::vector<containers::Container> last_containers_;
};

template <>
void BrowserContextKeyedAPIFactory<
    ContextualIdentitiesAPI>::DeclareFactoryDependencies();

// Their histogram values are UNKNOWN, so that the upstreams' additions to that
// enum can't conflict with them.
class ContextualIdentitiesQueryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextualIdentities.query", UNKNOWN)

 protected:
  ~ContextualIdentitiesQueryFunction() override = default;
  ResponseAction Run() override;
};

class ContextualIdentitiesGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextualIdentities.get", UNKNOWN)

 protected:
  ~ContextualIdentitiesGetFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_IDENTITIES_CONTEXTUAL_IDENTITIES_API_H_
