// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_PROXY_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_PROXY_INTERNAL_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension_id.h"
#include "net/proxy_resolution/proxy_list.h"
#include "url/gurl.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// proxy.onRequest. While an add-on listens, the profile's requests take their
// proxy from here (firefox_proxy_config.h): proxy.onRequest goes to each
// listening add-on in turn that may see the request's URL, and each of its
// listeners' answers builds on the one before, as in Firefox (ApplyAnswer()
// in the .cc). An answer Firefox refuses goes to that add-on's proxy.onError,
// and changes nothing.
class FirefoxProxyRequests : public BrowserContextKeyedAPI,
                             public EventRouter::Observer {
 public:
  // The proxies to try in turn.
  using Answer = base::OnceCallback<void(net::ProxyList)>;

  explicit FirefoxProxyRequests(content::BrowserContext* context);
  FirefoxProxyRequests(const FirefoxProxyRequests&) = delete;
  FirefoxProxyRequests& operator=(const FirefoxProxyRequests&) = delete;
  ~FirefoxProxyRequests() override;

  static BrowserContextKeyedAPIFactory<FirefoxProxyRequests>*
  GetFactoryInstance();
  // The original profile's, for an incognito one too. Null in unit tests.
  static FirefoxProxyRequests* Get(content::BrowserContext* context);

  // Whether an add-on listens that may see the requests of `context`: in an
  // incognito profile, one allowed in incognito.
  bool Listening(content::BrowserContext* context) const;

  // Runs `callback` whenever Listening() may have changed.
  base::CallbackListSubscription OnListeningChanged(
      base::RepeatingClosure callback);

  // Asks the listening add-ons for the proxy of a request of `context` to
  // `url`. `answer` runs once, at the latest when the service shuts down.
  // `own` is the browser's own proxies for it, which the first answer builds
  // on, as in Firefox. Firefox asks once for each request, where the browser
  // may resolve one request's proxy twice: an ask for the same URL while one
  // is waiting for the add-ons, or soon after one started, takes its answer
  // (kSameRequest in the .cc).
  void Ask(content::BrowserContext* context,
           const GURL& url,
           net::ProxyList own,
           Answer answer);

  // The answers of `extension_id`'s listeners to request `request_id`, in
  // their order, each Firefox's ProxyInfo, a list of them or null; and the
  // messages of the errors their promises rejected with.
  void OnAnswer(const ExtensionId& extension_id,
                int request_id,
                const base::ListValue& answers,
                const std::vector<std::string>& errors);

 private:
  friend class BrowserContextKeyedAPIFactory<FirefoxProxyRequests>;

  static const char* service_name() { return "FirefoxProxyRequests"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  struct Request;

  // KeyedService:
  void Shutdown() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // The add-ons whose listeners may see `context`'s requests to `url`, or,
  // for an empty `url`, any of its requests.
  std::vector<ExtensionId> Listeners(content::BrowserContext* context,
                                     const GURL& url) const;
  void AskNext(int request_id);
  void Finish(int request_id);
  void DispatchError(const ExtensionId& extension_id,
                     const std::string& message);

  raw_ptr<content::BrowserContext> context_;
  base::RepeatingClosureList listening_changed_;
  std::map<int, std::unique_ptr<Request>> requests_;
  // Those answered lately, for kSameRequest.
  std::vector<std::unique_ptr<Request>> answered_;
  int next_request_id_ = 1;
};

template <>
void BrowserContextKeyedAPIFactory<
    FirefoxProxyRequests>::DeclareFactoryDependencies();

// What the custom binding (proxy_custom_bindings.js) reports of an add-on's
// proxy.onRequest listeners. Its histogram value is UNKNOWN, so that the
// upstreams' additions to that enum can't conflict with it.
class ProxyInternalReportResultFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("proxyInternal.reportResult", UNKNOWN)

 protected:
  ~ProxyInternalReportResultFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_INTERNAL_PROXY_INTERNAL_API_H_
