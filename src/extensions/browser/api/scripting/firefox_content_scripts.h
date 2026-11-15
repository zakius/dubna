// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_API_SCRIPTING_FIREFOX_CONTENT_SCRIPTS_H_
#define EXTENSIONS_BROWSER_API_SCRIPTING_FIREFOX_CONTENT_SCRIPTS_H_

#include <optional>
#include <string>
#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// contentScripts.register(). Its scripts are dynamic content scripts, as
// scripting.registerContentScripts() makes, with IDs of their own, and they
// take code as well as files. As in Firefox, they last until the document that
// registered them goes away, and the scripting API doesn't list or remove them.
namespace firefox_content_scripts {

// Whether `id`, with its dynamic content script prefix, is a contentScripts
// script.
bool IsFirefoxScriptId(const std::string& id);

// The IDs, without their prefix, of the extension's scripting API scripts:
// what scripting.unregisterContentScripts() without IDs removes.
std::vector<std::string> ScriptingScriptIds(content::BrowserContext* context,
                                            const ExtensionId& extension_id);

}  // namespace firefox_content_scripts

// Registers the scripts and responds with their ID, which the custom binding
// (content_scripts_custom_bindings.js) turns into Firefox's object with
// unregister(). The histogram values are UNKNOWN, so that the upstreams'
// additions to that enum can't conflict with them.
class ContentScriptsRegisterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentScripts.register", UNKNOWN)

 protected:
  ~ContentScriptsRegisterFunction() override = default;
  ResponseAction Run() override;

 private:
  void OnFilesValidated(scripting::ValidateScriptsResult result);
  void OnRegistered(const std::optional<std::string>& error);

  std::string id_;
};

class ContentScriptsInternalUnregisterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contentScriptsInternal.unregister", UNKNOWN)

 protected:
  ~ContentScriptsInternalUnregisterFunction() override = default;
  ResponseAction Run() override;

 private:
  void OnUnregistered(const std::optional<std::string>& error);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SCRIPTING_FIREFOX_CONTENT_SCRIPTS_H_
