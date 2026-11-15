// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// proxy.onRequest, whose listeners answer with the request's proxy, a list of
// them, or a promise of either. The browser sends the request's ID before
// Firefox's details (proxy_internal_api.h): this passes the details on and
// reports the answer, and the first error a listener's promise rejected with.

const proxyInternal = getInternalApi('proxyInternal');

function errorMessage(error) {
  return String(error instanceof Error ? error.message : error);
}

bindingUtil.registerEventArgumentMassager(
    'proxy.onRequest', function(args, dispatch) {
      const requestId = args[0];
      // The answers of the listeners that returned something, in order. A
      // listener that threw is reported as any listener's error is.
      const dispatched = dispatch([args[1]]);
      const answers = dispatched ? dispatched.results : [];
      // As in Firefox, each answer that isn't empty replaces the one before,
      // in the listeners' order, however their promises settle.
      let result = null;
      let error;
      let settled = Promise.resolve();
      for (const answer of answers) {
        settled = settled.then(() => answer).then(value => {
          if (value !== undefined && value !== null) {
            result = value;
          }
        }, reason => {
          error ??= errorMessage(reason);
        });
      }
      settled.then(() => {
        try {
          proxyInternal.reportResult(requestId, result, error);
        } catch (refused) {
          // An answer that can't be sent, such as NaN.
          proxyInternal.reportResult(requestId, null, errorMessage(refused));
        }
      });
    });

// proxy.onError's listeners get an Error, as in Firefox.
bindingUtil.registerEventArgumentMassager(
    'proxy.onError', function(args, dispatch) {
      dispatch([new Error(args[0].message)]);
    });
