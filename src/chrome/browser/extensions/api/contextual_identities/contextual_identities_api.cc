// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/contextual_identities/contextual_identities_api.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/containers/containers_service_factory.h"
#include "chrome/browser/extensions/firefox_cookie_store.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/firefox_content_script_stores.h"
#include "extensions/common/event_args.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

namespace {

constexpr char kOnCreated[] = "contextualIdentities.onCreated";
constexpr char kOnUpdated[] = "contextualIdentities.onUpdated";
constexpr char kOnRemoved[] = "contextualIdentities.onRemoved";

// Firefox's messages.
constexpr char kIdentitiesDisabled[] =
    "Contextual identities are currently disabled";
constexpr char kInvalidIdentity[] = "Invalid contextual identity: ";

containers::ContainersService* ServiceFor(content::BrowserContext* context) {
  return containers::ContainersServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context)->GetOriginalProfile());
}

// For contentScripts.register()'s cookieStoreId: the containers that
// contextualIdentities.get() finds.
bool HasContainer(content::BrowserContext* context, std::string_view id) {
  containers::ContainersService* service = ServiceFor(context);
  return service && service->Get(id).has_value();
}

// Firefox's ContextualIdentity.
base::DictValue Identity(const containers::Container& container) {
  SkColor color = container.color_value();
  return base::DictValue()
      .Set("name", container.name)
      .Set("icon", container.icon)
      // Firefox's own resource URL, which add-ons read the icon name from or
      // show the icon with. Only Firefox loads it.
      .Set("iconUrl", base::StrCat({"resource://usercontext-content/",
                                    container.icon, ".svg"}))
      .Set("color", container.color)
      .Set("colorCode",
           base::StringPrintf("#%02x%02x%02x", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color)))
      .Set("cookieStoreId",
           base::StrCat({kFirefoxContainerPrefix, container.id}));
}

bool SameLook(const containers::Container& a, const containers::Container& b) {
  return a.name == b.name && a.icon == b.icon && a.color == b.color;
}

const containers::Container* Find(
    const std::vector<containers::Container>& list,
    const std::string& id) {
  for (const containers::Container& container : list) {
    if (container.id == id) {
      return &container;
    }
  }
  return nullptr;
}

}  // namespace

ContextualIdentitiesAPI::ContextualIdentitiesAPI(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  firefox_content_scripts::SetHasContainer(&HasContainer);
  containers::ContainersService* service = ServiceFor(context);
  if (service) {
    last_containers_ = service->GetContainers();
    observation_.Observe(service);
  }
}

ContextualIdentitiesAPI::~ContextualIdentitiesAPI() = default;

// static
BrowserContextKeyedAPIFactory<ContextualIdentitiesAPI>*
ContextualIdentitiesAPI::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<ContextualIdentitiesAPI>>
      factory;
  return factory.get();
}

void ContextualIdentitiesAPI::OnContainersChanged() {
  std::vector<containers::Container> containers =
      observation_.GetSource()->GetContainers();
  for (const containers::Container& old : last_containers_) {
    if (!Find(containers, old.id)) {
      Dispatch(kOnRemoved, old);
    }
  }
  for (const containers::Container& container : containers) {
    const containers::Container* old = Find(last_containers_, container.id);
    if (!old) {
      Dispatch(kOnCreated, container);
    } else if (!SameLook(*old, container)) {
      Dispatch(kOnUpdated, container);
    }
  }
  last_containers_ = std::move(containers);
}

void ContextualIdentitiesAPI::Dispatch(const char* event_name,
                                       const containers::Container& container) {
  EventRouter* router = EventRouter::Get(profile_);
  if (!router || !router->HasEventListener(event_name)) {
    return;
  }
  base::ListValue args;
  args.Append(base::DictValue().Set("contextualIdentity", Identity(container)));
  // A stub histogram value, as theme.onUpdated's: one of our own could clash
  // with the upstreams' additions to that enum, and UNKNOWN fails a DCHECK.
  router->BroadcastEvent(
      std::make_unique<Event>(
          events::FOR_TEST, event_name,
          base::MakeRefCounted<EventArgs>(std::move(args))));
}

template <>
void BrowserContextKeyedAPIFactory<
    ContextualIdentitiesAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(containers::ContainersServiceFactory::GetInstance());
}

ExtensionFunction::ResponseAction ContextualIdentitiesQueryFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_dict());
  const std::string* name = args()[0].GetDict().FindString("name");
  containers::ContainersService* service = ServiceFor(browser_context());
  if (!service) {
    return RespondNow(Error(kIdentitiesDisabled));
  }
  base::ListValue identities;
  for (const containers::Container& container : service->GetContainers()) {
    if (!name || *name == container.name) {
      identities.Append(Identity(container));
    }
  }
  return RespondNow(WithArguments(std::move(identities)));
}

ExtensionFunction::ResponseAction ContextualIdentitiesGetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_string());
  const std::string& cookie_store_id = args()[0].GetString();
  containers::ContainersService* service = ServiceFor(browser_context());
  if (!service) {
    return RespondNow(Error(kIdentitiesDisabled));
  }
  std::optional<std::string> id =
      ContainerOfFirefoxCookieStore(cookie_store_id);
  std::optional<containers::Container> container =
      id ? service->Get(*id) : std::nullopt;
  if (!container) {
    return RespondNow(Error(base::StrCat({kInvalidIdentity, cookie_store_id})));
  }
  return RespondNow(WithArguments(Identity(*container)));
}

}  // namespace extensions
