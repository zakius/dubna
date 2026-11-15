// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// proxy.onRequest, whose listeners answer with the request's proxy, a list of
// them, null, or a promise of any of these. The browser sends the request's
// ID before Firefox's details (proxy_internal_api.h): this passes the details
// on and reports the listeners' answers in their order, however their
// promises settle, and the errors they rejected with. The browser builds the
// proxy from them as Firefox does.

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
      const reported = [];
      const errors = [];
      let settled = Promise.resolve();
      for (const answer of answers) {
        settled = settled.then(() => answer).then(value => {
          // As in Firefox, undefined is no answer, and null is one.
          if (value !== undefined) {
            reported.push(value);
          }
        }, reason => {
          errors.push(errorMessage(reason));
        });
      }
      settled.then(() => {
        try {
          proxyInternal.reportResult(requestId, reported, errors);
        } catch (refused) {
          // Answers that can't be sent, such as NaN.
          proxyInternal.reportResult(
              requestId, [], [...errors, errorMessage(refused)]);
        }
      });
    });

// proxy.onError's listeners get an Error, as in Firefox.
bindingUtil.registerEventArgumentMassager(
    'proxy.onError', function(args, dispatch) {
      dispatch([new Error(args[0].message)]);
    });
