// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/proxy_internal/firefox_proxy_config.h"

#include <map>
#include <utility>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/api/proxy_internal/proxy_internal_api.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "url/gurl.h"

namespace extensions::firefox_proxy {

namespace {

// The script's first line, then the wrapper's token.
constexpr char kScriptStart[] = "// Firefox add-ons: proxy.onRequest ";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("firefox_add_ons_proxy", R"(
      semantics {
        sender: "Firefox add-ons"
        description:
          "Proxy settings that ask a Firefox add-on, through Firefox's "
          "proxy.onRequest, which proxy each request goes through."
        trigger: "An installed add-on listens to proxy.onRequest."
        data: "None: these are settings, not a request."
        destination: OTHER
      }
      policy {
        cookies_allowed: NO
        setting: "Remove or disable the add-on."
        policy_exception_justification: "Not implemented."
      })");

class ConfigService;

// The wrappers by their token. On the UI thread, as the wrappers and the
// resolver factory are.
std::map<std::string, base::WeakPtr<ConfigService>>& Wrappers() {
  static base::NoDestructor<std::map<std::string, base::WeakPtr<ConfigService>>>
      wrappers;
  return *wrappers;
}

class ConfigService : public net::ProxyConfigService,
                      public net::ProxyConfigService::Observer {
 public:
  ConfigService(content::BrowserContext* context,
                std::unique_ptr<net::ProxyConfigService> inner)
      : context_(context),
        inner_(std::move(inner)),
        token_(base::UnguessableToken::Create().ToString()) {
    Wrappers()[token_] = weak_factory_.GetWeakPtr();
    inner_->AddObserver(this);
    if (FirefoxProxyRequests* requests = FirefoxProxyRequests::Get(context)) {
      listening_ = requests->Listening(context);
      subscription_ = requests->OnListeningChanged(base::BindRepeating(
          &ConfigService::OnListeningChanged, base::Unretained(this)));
    }
  }

  ConfigService(const ConfigService&) = delete;
  ConfigService& operator=(const ConfigService&) = delete;

  ~ConfigService() override {
    inner_->RemoveObserver(this);
    Wrappers().erase(token_);
  }

  // net::ProxyConfigService:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override {
    if (!listening_) {
      return inner_->GetLatestProxyConfig(config);
    }
    std::string script =
        base::StrCat({kScriptStart, token_,
                      "\nfunction FindProxyForURL(url, host) {\n"
                      "  return 'DIRECT';\n}\n"});
    *config = net::ProxyConfigWithAnnotation(
        net::ProxyConfig::CreateFromCustomPacURL(
            GURL(base::StrCat({"data:application/x-ns-proxy-autoconfig;base64,",
                               base::Base64Encode(script)}))),
        kTrafficAnnotation);
    return CONFIG_VALID;
  }
  void OnLazyPoll() override { inner_->OnLazyPoll(); }
  bool UsesPolling() override { return inner_->UsesPolling(); }

  // net::ProxyConfigService::Observer:
  void OnProxyConfigChanged(const net::ProxyConfigWithAnnotation& config,
                            ConfigAvailability availability) override {
    if (!listening_) {
      for (Observer& observer : observers_) {
        observer.OnProxyConfigChanged(config, availability);
      }
    }
  }

  // Asks the add-ons for the proxy of a request to `url`, building on the
  // browser's own. As in Firefox, a host on the browser's proxy exceptions
  // goes direct, without them.
  void Resolve(const GURL& url,
               base::OnceCallback<void(const net::ProxyInfo&)> callback) {
    net::ProxyInfo own = Fallback(url);
    FirefoxProxyRequests* requests = FirefoxProxyRequests::Get(context_);
    if (!requests || own.did_bypass_proxy()) {
      std::move(callback).Run(own);
      return;
    }
    requests->Ask(context_, url, own.proxy_list(),
                  base::BindOnce(&ConfigService::OnAnswer, std::move(callback)));
  }

 private:
  static void OnAnswer(base::OnceCallback<void(const net::ProxyInfo&)> callback,
                       net::ProxyList proxies) {
    net::ProxyInfo info;
    info.UseProxyList(proxies);
    std::move(callback).Run(info);
  }

  // The browser's own fixed proxies for `url`, or direct.
  net::ProxyInfo Fallback(const GURL& url) {
    net::ProxyInfo info;
    info.UseDirect();
    net::ProxyConfigWithAnnotation config;
    if (inner_->GetLatestProxyConfig(&config) == CONFIG_VALID) {
      config.value().proxy_rules().Apply(url, &info);
    }
    return info;
  }

  void OnListeningChanged() {
    FirefoxProxyRequests* requests = FirefoxProxyRequests::Get(context_);
    bool listening = requests && requests->Listening(context_);
    if (listening == listening_) {
      return;
    }
    listening_ = listening;
    net::ProxyConfigWithAnnotation config;
    ConfigAvailability availability = GetLatestProxyConfig(&config);
    if (availability == CONFIG_PENDING) {
      return;
    }
    for (Observer& observer : observers_) {
      observer.OnProxyConfigChanged(config, availability);
    }
  }

  raw_ptr<content::BrowserContext> context_;
  std::unique_ptr<net::ProxyConfigService> inner_;
  const std::string token_;
  bool listening_ = false;
  base::ObserverList<Observer>::Unchecked observers_;
  base::CallbackListSubscription subscription_;
  base::WeakPtrFactory<ConfigService> weak_factory_{this};
};

class Resolver : public proxy_resolver::mojom::ProxyResolver {
 public:
  Resolver(base::WeakPtr<ConfigService> service,
           mojo::PendingRemote<
               proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
               factory_client)
      : service_(std::move(service)),
        factory_client_(std::move(factory_client)) {
    factory_client_->ReportResult(net::OK);
  }

  // proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
          client) override {
    auto report = base::BindOnce(
        [](mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient>
               client,
           const net::ProxyInfo& info) { client->ReportResult(net::OK, info); },
        mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient>(
            std::move(client)));
    if (!service_) {
      net::ProxyInfo direct;
      direct.UseDirect();
      std::move(report).Run(direct);
      return;
    }
    service_->Resolve(url, std::move(report));
  }

 private:
  base::WeakPtr<ConfigService> service_;
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
      factory_client_;
};

}  // namespace

std::unique_ptr<net::ProxyConfigService> WrapConfigService(
    content::BrowserContext* context,
    std::unique_ptr<net::ProxyConfigService> config_service) {
  return std::make_unique<ConfigService>(context, std::move(config_service));
}

bool IsScript(const std::string& pac_script) {
  return base::StartsWith(pac_script, kScriptStart);
}

void CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client) {
  std::string_view rest = std::string_view(pac_script).substr(
      std::string_view(kScriptStart).size());
  std::string token(rest.substr(0, rest.find('\n')));
  auto it = Wrappers().find(token);
  base::WeakPtr<ConfigService> service;
  if (it != Wrappers().end()) {
    service = it->second;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Resolver>(std::move(service), std::move(client)),
      std::move(receiver));
}

}  // namespace extensions::firefox_proxy
