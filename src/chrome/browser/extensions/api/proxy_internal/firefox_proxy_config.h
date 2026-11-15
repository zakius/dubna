// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_FIREFOX_PROXY_CONFIG_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_FIREFOX_PROXY_CONFIG_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom-forward.h"

namespace content {
class BrowserContext;
}

namespace net {
class ProxyConfigService;
}

// Firefox add-ons (dubna's webextensions/README.md): how a request's proxy
// reaches proxy.onRequest (proxy_internal_api.h). The network service resolves
// proxies from what the profile's ProxyConfigMonitor sends it. While an add-on
// listens, the wrapper below sends a PAC script instead, which names it; the
// network service hands that script to ChromeMojoProxyResolverFactory, which
// passes it here, and each request's proxy is then asked of the add-ons. When
// none answers, the browser's own settings apply, as far as they are fixed
// proxies: a PAC script or auto-detection of the user's goes direct then.
namespace extensions::firefox_proxy {

// Wraps a profile's proxy settings as described above.
std::unique_ptr<net::ProxyConfigService> WrapConfigService(
    content::BrowserContext* context,
    std::unique_ptr<net::ProxyConfigService> config_service);

// Whether `pac_script` is the one the wrapper sends.
bool IsScript(const std::string& pac_script);

// A resolver for that script, which asks the add-ons.
void CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client);

}  // namespace extensions::firefox_proxy

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_FIREFOX_PROXY_CONFIG_H_
