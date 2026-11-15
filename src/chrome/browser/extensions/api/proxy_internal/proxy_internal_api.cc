// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/proxy_internal/proxy_internal_api.h"

#include <algorithm>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_cookie_store.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"

namespace extensions {

namespace {

constexpr char kOnRequest[] = "proxy.onRequest";
constexpr char kOnError[] = "proxy.onError";

// How long an add-on may take to answer before the next one is asked. Firefox
// waits for ever; here a request whose event never reaches the add-on would
// hang.
constexpr base::TimeDelta kAnswerTimeout = base::Seconds(20);

// Firefox's port as parseInt() reads it: a number, or a string's leading
// digits.
std::optional<int> ReadPort(const base::Value* value) {
  if (!value) {
    return std::nullopt;
  }
  if (value->is_int()) {
    return value->GetInt();
  }
  if (value->is_double()) {
    double port = value->GetDouble();
    if (port > -1e6 && port < 1e6) {
      return static_cast<int>(port);
    }
    return std::nullopt;
  }
  if (!value->is_string()) {
    return std::nullopt;
  }
  std::string_view text =
      base::TrimWhitespaceASCII(value->GetString(), base::TRIM_LEADING);
  size_t end = (!text.empty() && (text[0] == '-' || text[0] == '+')) ? 1 : 0;
  while (end < text.size() && end < 8 && base::IsAsciiDigit(text[end])) {
    ++end;
  }
  int port;
  if (!base::StringToInt(text.substr(0, end), &port)) {
    return std::nullopt;
  }
  return port;
}

// One of Firefox's ProxyInfo, as its ProxyInfoData checks it, with its
// messages. Its username, password, proxyDNS, failoverTimeout,
// proxyAuthorizationHeader and connectionIsolationKey aren't followed.
std::optional<net::ProxyChain> ReadProxy(const base::Value& value,
                                         std::string* error) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    *error = "ProxyInfoData: proxyData must be an object or array of objects";
    return std::nullopt;
  }
  const std::string* type = dict->FindString("type");
  std::string lower = type ? base::ToLowerASCII(*type) : std::string();
  if (lower == "direct") {
    return net::ProxyChain::Direct();
  }
  net::ProxyServer::Scheme scheme;
  if (lower == "http" || lower == "proxy") {
    scheme = net::ProxyServer::SCHEME_HTTP;
  } else if (lower == "https") {
    scheme = net::ProxyServer::SCHEME_HTTPS;
  } else if (lower == "socks") {
    scheme = net::ProxyServer::SCHEME_SOCKS5;
  } else if (lower == "socks4") {
    scheme = net::ProxyServer::SCHEME_SOCKS4;
  } else {
    *error = base::StrCat({"ProxyInfoData: Invalid proxy server type: \"",
                           type ? *type : "undefined", "\""});
    return std::nullopt;
  }
  const std::string* host = dict->FindString("host");
  if (!host || host->find(' ') != std::string::npos) {
    *error = base::StrCat({"ProxyInfoData: Invalid proxy server host: \"",
                           host ? *host : "undefined", "\""});
    return std::nullopt;
  }
  if (host->empty()) {
    *error = "ProxyInfoData: Proxy server host cannot be empty";
    return std::nullopt;
  }
  std::optional<int> port = ReadPort(dict->Find("port"));
  if (!port) {
    *error = "ProxyInfoData: Invalid proxy server port: \"NaN\"";
    return std::nullopt;
  }
  if (*port < 1 || *port > 0xffff) {
    *error = base::StrCat({"ProxyInfoData: Proxy server port ",
                           base::NumberToString(*port),
                           " outside range 1 to 65535"});
    return std::nullopt;
  }
  net::ProxyServer server = net::ProxySchemeHostAndPortToProxyServer(
      scheme, net::HostPortPair(*host, *port).ToString());
  if (!server.is_valid()) {
    *error = base::StrCat(
        {"ProxyInfoData: Invalid proxy server host: \"", *host, "\""});
    return std::nullopt;
  }
  return net::ProxyChain(std::move(server));
}

// A listener's answer: one ProxyInfo, or a list of them to try in turn. An
// empty list is no answer.
std::optional<net::ProxyList> ReadProxies(const base::Value& value,
                                          std::string* error) {
  net::ProxyList proxies;
  if (value.is_list()) {
    for (const base::Value& item : value.GetList()) {
      std::optional<net::ProxyChain> chain = ReadProxy(item, error);
      if (!chain) {
        return std::nullopt;
      }
      proxies.AddProxyChain(std::move(*chain));
    }
  } else {
    std::optional<net::ProxyChain> chain = ReadProxy(value, error);
    if (!chain) {
      return std::nullopt;
    }
    proxies.AddProxyChain(std::move(*chain));
  }
  return proxies;
}

}  // namespace

struct FirefoxProxyRequests::Request {
  GURL url;
  bool incognito = false;
  // The add-ons still to ask, the one asked now, and the last answer.
  std::vector<ExtensionId> left;
  ExtensionId asked;
  std::optional<net::ProxyList> proxies;
  Answer answer;
  base::OneShotTimer timeout;
};

FirefoxProxyRequests::FirefoxProxyRequests(content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context)->RegisterObserver(this, kOnRequest);
}

FirefoxProxyRequests::~FirefoxProxyRequests() = default;

// static
BrowserContextKeyedAPIFactory<FirefoxProxyRequests>*
FirefoxProxyRequests::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<FirefoxProxyRequests>>
      factory;
  return factory.get();
}

// static
FirefoxProxyRequests* FirefoxProxyRequests::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

void FirefoxProxyRequests::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
  while (!requests_.empty()) {
    Finish(requests_.begin()->first, std::nullopt);
  }
}

void FirefoxProxyRequests::OnListenerAdded(const EventListenerInfo& details) {
  listening_changed_.Notify();
}

void FirefoxProxyRequests::OnListenerRemoved(const EventListenerInfo& details) {
  listening_changed_.Notify();
}

bool FirefoxProxyRequests::Listening(content::BrowserContext* context) const {
  return !Listeners(context, GURL()).empty();
}

base::CallbackListSubscription FirefoxProxyRequests::OnListeningChanged(
    base::RepeatingClosure callback) {
  return listening_changed_.Add(std::move(callback));
}

std::vector<ExtensionId> FirefoxProxyRequests::Listeners(
    content::BrowserContext* context,
    const GURL& url) const {
  std::vector<ExtensionId> ids;
  const ExtensionSet& enabled =
      ExtensionRegistry::Get(context_)->enabled_extensions();
  for (const auto& listener :
       EventRouter::Get(context_)->listeners().GetEventListenersByName(
           kOnRequest)) {
    const ExtensionId& id = listener->extension_id();
    const Extension* extension = enabled.GetByID(id);
    if (!extension || std::ranges::find(ids, id) != ids.end()) {
      continue;
    }
    if (context->IsOffTheRecord() && !util::IsIncognitoEnabled(id, context)) {
      continue;
    }
    // As in Firefox, an add-on sees the requests to its host permissions.
    if (!url.is_empty() &&
        !extension->permissions_data()->HasHostPermission(url)) {
      continue;
    }
    ids.push_back(id);
  }
  return ids;
}

void FirefoxProxyRequests::Ask(content::BrowserContext* context,
                               const GURL& url,
                               Answer answer) {
  int request_id = next_request_id_++;
  auto request = std::make_unique<Request>();
  request->url = url;
  request->incognito = context->IsOffTheRecord();
  request->left = Listeners(context, url);
  request->answer = std::move(answer);
  requests_[request_id] = std::move(request);
  AskNext(request_id);
}

void FirefoxProxyRequests::AskNext(int request_id) {
  Request& request = *requests_[request_id];
  if (request.left.empty()) {
    Finish(request_id, std::move(request.proxies));
    return;
  }
  request.asked = request.left.front();
  request.left.erase(request.left.begin());
  request.timeout.Start(
      FROM_HERE, kAnswerTimeout,
      base::BindOnce(&FirefoxProxyRequests::AskNext, base::Unretained(this),
                     request_id));
  // Firefox's details of the request, as far as the proxy resolution knows
  // them: the URL (of an https: one only the origin), and no tab or type.
  base::ListValue args;
  args.Append(request_id);
  args.Append(
      base::DictValue()
          .Set("requestId", base::NumberToString(request_id))
          .Set("url", request.url.spec())
          .Set("type", "other")
          .Set("tabId", -1)
          .Set("frameId", -1)
          .Set("parentFrameId", -1)
          .Set("timeStamp",
               base::Time::Now().InMillisecondsFSinceUnixEpoch())
          .Set("incognito", request.incognito)
          .Set("thirdParty", false)
          .Set("fromCache", false)
          .Set("cookieStoreId", request.incognito ? kFirefoxPrivateCookieStore
                                                  : kFirefoxDefaultCookieStore));
  // A stub histogram value, as theme.onUpdated's: one of our own could clash
  // with the upstreams' additions to that enum, and UNKNOWN fails a DCHECK.
  EventRouter::Get(context_)->DispatchEventToExtension(
      request.asked,
      std::make_unique<Event>(events::FOR_TEST, kOnRequest, std::move(args)));
}

void FirefoxProxyRequests::OnAnswer(const ExtensionId& extension_id,
                                    int request_id,
                                    const base::Value* result,
                                    const std::string* error) {
  auto it = requests_.find(request_id);
  // Only the add-on asked may answer, once.
  if (it == requests_.end() || it->second->asked != extension_id) {
    return;
  }
  Request& request = *it->second;
  request.asked.clear();
  if (error) {
    DispatchError(extension_id, *error);
  }
  if (result && !result->is_none()) {
    std::string message;
    std::optional<net::ProxyList> proxies = ReadProxies(*result, &message);
    if (!proxies) {
      DispatchError(extension_id, message);
    } else if (!proxies->IsEmpty()) {
      request.proxies = std::move(proxies);
    }
  }
  AskNext(request_id);
}

void FirefoxProxyRequests::Finish(int request_id,
                                  std::optional<net::ProxyList> proxies) {
  auto it = requests_.find(request_id);
  Answer answer = std::move(it->second->answer);
  requests_.erase(it);
  std::move(answer).Run(std::move(proxies));
}

void FirefoxProxyRequests::DispatchError(const ExtensionId& extension_id,
                                         const std::string& message) {
  base::ListValue args;
  args.Append(base::DictValue().Set("message", message));
  EventRouter::Get(context_)->DispatchEventToExtension(
      extension_id,
      std::make_unique<Event>(events::FOR_TEST, kOnError, std::move(args)));
}

template <>
void BrowserContextKeyedAPIFactory<
    FirefoxProxyRequests>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ExtensionFunction::ResponseAction ProxyInternalReportResultFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_int());
  const base::Value* result =
      args().size() > 1 && !args()[1].is_none() ? &args()[1] : nullptr;
  const std::string* error =
      args().size() > 2 && args()[2].is_string() ? &args()[2].GetString()
                                                 : nullptr;
  if (FirefoxProxyRequests* requests =
          FirefoxProxyRequests::Get(browser_context())) {
    requests->OnAnswer(extension_id(), args()[0].GetInt(), result, error);
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
