// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// contentScripts.register() resolves to an object whose unregister() removes
// the scripts. The browser answers with the scripts' ID
// (firefox_content_scripts.cc), which this turns into that object.

const contentScriptsInternal = getInternalApi('contentScriptsInternal');

function registeredContentScript(scriptId) {
  let unregistered = false;
  return {
    unregister() {
      return new Promise((resolve, reject) => {
        if (unregistered) {
          // Firefox's own message.
          reject(new Error('Content script already unregistered'));
          return;
        }
        unregistered = true;
        contentScriptsInternal.unregister(scriptId, () => resolve());
      });
    },
  };
}

apiBridge.registerCustomHook(function(bindingsAPI) {
  bindingsAPI.apiFunctions.setCustomCallback(
      'register', function(callback, scriptId) {
        if (!callback) {
          return;
        }
        // An error has no ID: the callback rejects the promise, or sets
        // runtime.lastError.
        if (scriptId === undefined) {
          callback();
          return;
        }
        callback(registeredContentScript(scriptId));
      });
});
