// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// proxy.onRequest, whose listeners answer with the request's proxy, a list of
// them, or a promise of either. The browser sends the request's ID before
// Firefox's details (proxy_internal_api.h): this passes the details on and
// reports the first answer, or the error its promise rejected with.

const proxyInternal = getInternalApi('proxyInternal');

bindingUtil.registerEventArgumentMassager(
    'proxy.onRequest', function(args, dispatch) {
      const requestId = args[0];
      // The listeners that returned something, in order. A listener that
      // threw is reported as any listener's error is.
      const dispatched = dispatch([args[1]]);
      const answer = dispatched ? dispatched.results[0] : undefined;
      Promise.resolve(answer).then(
          result => proxyInternal.reportResult(requestId, result ?? null),
          error => proxyInternal.reportResult(
              requestId, null,
              String(error instanceof Error ? error.message : error)));
    });

// proxy.onError's listeners get an Error, as in Firefox.
bindingUtil.registerEventArgumentMassager(
    'proxy.onError', function(args, dispatch) {
      dispatch([new Error(args[0].message)]);
    });
