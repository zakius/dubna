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
  // Firefox takes cssOrigin in any case ("USER"), lowercased before it checks
  // the value. The add-on's own object stays as it is.
  bindingsAPI.apiFunctions.setUpdateArgumentsPreValidate(
      'register', function(options) {
        const args = $Array.slice(arguments);
        if (options && typeof options.cssOrigin === 'string') {
          args[0] = $Object.assign({}, options, {
            cssOrigin: $String.toLowerCase(options.cssOrigin),
          });
        }
        return args;
      });
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
