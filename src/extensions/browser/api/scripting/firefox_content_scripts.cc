// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/api/scripting/firefox_content_scripts.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/match_origin_as_fallback.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

// Follows the dynamic content script prefix. The scripting API refuses IDs
// that start with "_", so none of its IDs looks like this.
constexpr char kIdPrefix[] = "_firefox_";

std::string FirefoxPrefix() {
  return base::StrCat({UserScript::kDynamicContentScriptPrefix, kIdPrefix});
}

// The extension's script loader, or null if the extension isn't enabled (its
// loader is gone, or belongs to its next load).
ExtensionUserScriptLoader* GetLoader(content::BrowserContext* context,
                                     const ExtensionId& extension_id) {
  if (!ExtensionRegistry::Get(context)->enabled_extensions().Contains(
          extension_id)) {
    return nullptr;
  }
  return ExtensionSystem::Get(context)
      ->user_script_manager()
      ->GetUserScriptLoaderForExtension(extension_id);
}

// Removes those of `ids` that the loader still has.
void RemoveScripts(ExtensionUserScriptLoader* loader,
                   const std::set<std::string>& ids,
                   ExtensionUserScriptLoader::DynamicScriptsModifiedCallback
                       callback) {
  std::set<std::string> existing =
      loader->GetDynamicScriptIDs(UserScript::Source::kDynamicContentScript);
  std::set<std::string> to_remove;
  for (const std::string& id : ids) {
    if (existing.contains(id)) {
      to_remove.insert(id);
    }
  }
  loader->RemoveDynamicScripts(to_remove, std::move(callback));
}

// Removes a closed document's scripts. The profile may have gone in the
// meantime, or its services may have been shutting down with the document.
void RemoveScriptsOfClosedDocument(void* context,
                                   const ExtensionId& extension_id,
                                   const std::set<std::string>& ids) {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (!client || client->IsShuttingDown() || !client->IsValidContext(context)) {
    return;
  }
  ExtensionUserScriptLoader* loader =
      GetLoader(static_cast<content::BrowserContext*>(context), extension_id);
  if (loader) {
    RemoveScripts(loader, ids, base::DoNothing());
  }
}

// The scripts a document registered. Firefox unregisters them when the
// context that registered them closes: an event page that suspends, a popup,
// an options page that navigates.
class DocumentScripts : public content::DocumentUserData<DocumentScripts> {
 public:
  ~DocumentScripts() override {
    if (ids_.empty()) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RemoveScriptsOfClosedDocument,
                       static_cast<void*>(context_.get()), extension_id_,
                       std::move(ids_)));
  }

  void Add(const ExtensionId& extension_id, const std::string& id) {
    extension_id_ = extension_id;
    ids_.insert(id);
  }

 private:
  explicit DocumentScripts(content::RenderFrameHost* rfh)
      : content::DocumentUserData<DocumentScripts>(rfh),
        context_(rfh->GetBrowserContext()) {}

  friend content::DocumentUserData<DocumentScripts>;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Only compared against the live profiles once the document has gone.
  raw_ptr<content::BrowserContext, DisableDanglingPtrDetection> context_;
  ExtensionId extension_id_;
  std::set<std::string> ids_;
};

DOCUMENT_USER_DATA_KEY_IMPL(DocumentScripts);

// Adds the files and the code of a `js` or `css` list to `contents`. As in
// Firefox, a file's path is relative to `page`, the page that registers the
// scripts. Returns false if the list isn't one of Firefox's, and sets `error`
// for a file that Firefox refuses.
bool AddSources(const Extension& extension,
                const GURL& page,
                const base::Value* sources,
                UserScript::ContentList& contents,
                std::string* error) {
  if (!sources) {
    return true;
  }
  if (!sources->is_list()) {
    return false;
  }
  for (const base::Value& source : sources->GetList()) {
    if (!source.is_dict()) {
      return false;
    }
    const std::string* file = source.GetDict().FindString("file");
    const std::string* code = source.GetDict().FindString("code");
    if (file) {
      GURL url = page.Resolve(*file);
      if (!url.is_valid() ||
          !url::Origin::Create(url).IsSameOriginWith(extension.origin())) {
        // Firefox's message.
        *error = base::StrCat({"Access denied for URL ", *file});
        return true;
      }
      GURL::Replacements no_query;
      no_query.ClearQuery();
      no_query.ClearRef();
      url = url.ReplaceComponents(no_query);
      // The path without its leading "/".
      ExtensionResource resource = extension.GetResource(
          base::UnescapeBinaryURLComponent(std::string(url.path()).substr(1)));
      contents.push_back(UserScript::Content::CreateFile(
          resource.extension_root(), resource.relative_path(), url));
    } else if (code && !code->empty()) {
      // As the userScripts API does with code.
      std::unique_ptr<UserScript::Content> content =
          UserScript::Content::CreateInlineCode(extension.GetResourceURL(
              base::Uuid::GenerateRandomV4().AsLowercaseString()));
      content->set_content(*code);
      contents.push_back(std::move(content));
    }
  }
  return true;
}

// Returns the strings of `list`, or nullopt if it isn't a list of strings.
std::optional<std::vector<std::string>> Strings(const base::Value* list) {
  if (!list || !list->is_list()) {
    return std::nullopt;
  }
  std::vector<std::string> strings;
  for (const base::Value& item : list->GetList()) {
    if (!item.is_string()) {
      return std::nullopt;
    }
    strings.push_back(item.GetString());
  }
  return strings;
}

mojom::RunLocation RunLocation(const std::string* run_at) {
  if (run_at && *run_at == "document_start") {
    return mojom::RunLocation::kDocumentStart;
  }
  if (run_at && *run_at == "document_end") {
    return mojom::RunLocation::kDocumentEnd;
  }
  return mojom::RunLocation::kDocumentIdle;
}

}  // namespace

namespace firefox_content_scripts {

bool IsFirefoxScriptId(const std::string& id) {
  return base::StartsWith(id, FirefoxPrefix());
}

std::vector<std::string> ScriptingScriptIds(content::BrowserContext* context,
                                            const ExtensionId& extension_id) {
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(context)
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension_id);
  std::vector<std::string> ids;
  for (const std::string& id : loader->GetDynamicScriptIDs(
           UserScript::Source::kDynamicContentScript)) {
    if (!IsFirefoxScriptId(id)) {
      ids.push_back(UserScript::TrimPrefixFromScriptID(id));
    }
  }
  return ids;
}

}  // namespace firefox_content_scripts

ExtensionFunction::ResponseAction ContentScriptsRegisterFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_dict());
  const base::DictValue& options = args()[0].GetDict();
  const Extension& extension = *this->extension();

  id_ = base::StrCat(
      {FirefoxPrefix(), base::Uuid::GenerateRandomV4().AsLowercaseString()});
  auto script = std::make_unique<UserScript>();
  script->set_id(id_);
  script->set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));

  std::optional<std::vector<std::string>> matches =
      Strings(options.Find("matches"));
  EXTENSION_FUNCTION_VALIDATE(matches);
  std::optional<std::vector<std::string>> exclude_matches;
  if (options.Find("excludeMatches")) {
    exclude_matches = Strings(options.Find("excludeMatches"));
    EXTENSION_FUNCTION_VALIDATE(exclude_matches);
  }
  std::u16string error;
  if (!script_parsing::ParseMatchPatterns(
          *matches, base::OptionalToPtr(exclude_matches),
          extension.creation_flags(),
          /*can_execute_script_everywhere=*/false,
          /*all_urls_includes_chrome_urls=*/false,
          /*definition_index=*/std::nullopt, script.get(), &error,
          /*wants_file_access=*/nullptr)) {
    return RespondNow(Error(base::UTF16ToUTF8(error)));
  }

  std::optional<std::vector<std::string>> include_globs;
  if (options.Find("includeGlobs")) {
    include_globs = Strings(options.Find("includeGlobs"));
    EXTENSION_FUNCTION_VALIDATE(include_globs);
  }
  std::optional<std::vector<std::string>> exclude_globs;
  if (options.Find("excludeGlobs")) {
    exclude_globs = Strings(options.Find("excludeGlobs"));
    EXTENSION_FUNCTION_VALIDATE(exclude_globs);
  }
  script_parsing::ParseGlobs(base::OptionalToPtr(include_globs),
                             base::OptionalToPtr(exclude_globs), script.get());

  // A call without a page (none in Manifest V2) takes files from the root.
  const GURL& page = source_url().is_valid() ? source_url() : extension.url();
  std::string sources_error;
  EXTENSION_FUNCTION_VALIDATE(AddSources(extension, page, options.Find("js"),
                                         script->js_scripts(), &sources_error));
  EXTENSION_FUNCTION_VALIDATE(AddSources(extension, page, options.Find("css"),
                                         script->css_scripts(),
                                         &sources_error));
  if (!sources_error.empty()) {
    return RespondNow(Error(std::move(sources_error)));
  }

  script->set_match_all_frames(options.FindBool("allFrames").value_or(false));
  // As in Firefox, matchOriginAsFallback overrides matchAboutBlank.
  if (options.FindBool("matchOriginAsFallback").value_or(false)) {
    script->set_match_origin_as_fallback(
        mojom::MatchOriginAsFallbackBehavior::kAlways);
  } else if (options.FindBool("matchAboutBlank").value_or(false)) {
    script->set_match_origin_as_fallback(
        mojom::MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree);
  }
  if (!script_parsing::ValidateMatchOriginAsFallback(
          script->match_origin_as_fallback(), script->url_patterns(), &error)) {
    return RespondNow(Error(base::UTF16ToUTF8(error)));
  }
  script->set_run_location(RunLocation(options.FindString("runAt")));
  const std::string* world = options.FindString("world");
  script->set_execution_world(world && *world == "MAIN"
                                  ? mojom::ExecutionWorld::kMain
                                  : mojom::ExecutionWorld::kIsolated);
  // cssOrigin "user" and cookieStoreId are taken but not followed yet: the
  // styles are the page's author styles, and the scripts run in every
  // container.
  script->set_incognito_enabled(scripting::ScriptsShouldBeAllowedInIncognito(
      extension.id(), browser_context()));

  // Firefox registers a script with nothing to inject too, which does
  // nothing.
  if (script->js_scripts().empty() && script->css_scripts().empty()) {
    return RespondNow(WithArguments(id_));
  }

  if (render_frame_host()) {
    DocumentScripts::GetOrCreateForCurrentDocument(render_frame_host())
        ->Add(extension.id(), id_);
  }

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension.id());
  // Pending until the files are checked, so that an unregister() that comes
  // first stops the script from being added.
  loader->AddPendingDynamicScriptIDs({id_});

  UserScriptList scripts;
  scripts.push_back(std::move(script));
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&scripting::ValidateParsedScriptsOnFileThread,
                     script_parsing::GetSymlinkPolicy(&extension),
                     std::move(scripts)),
      base::BindOnce(&ContentScriptsRegisterFunction::OnFilesValidated, this));
  return RespondLater();
}

void ContentScriptsRegisterFunction::OnFilesValidated(
    scripting::ValidateScriptsResult result) {
  // As in the scripting API: nothing to answer once the profile or the
  // extension has gone.
  if (!browser_context()) {
    return;
  }
  ExtensionUserScriptLoader* loader =
      GetLoader(browser_context(), extension_id());
  if (!loader) {
    return;
  }
  if (result.second) {
    loader->RemovePendingDynamicScriptIDs({id_});
    Respond(Error(std::move(*result.second)));
    return;
  }
  // Scripts that don't persist: Firefox forgets them when the browser closes.
  loader->AddDynamicScripts(
      std::move(result.first), /*persistent_script_ids=*/{},
      base::BindOnce(&ContentScriptsRegisterFunction::OnRegistered, this));
}

void ContentScriptsRegisterFunction::OnRegistered(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }
  Respond(WithArguments(id_));
}

ExtensionFunction::ResponseAction
ContentScriptsInternalUnregisterFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_string());
  const std::string& id = args()[0].GetString();
  // Only contentScripts' own: the scripting API's are its to remove.
  if (!firefox_content_scripts::IsFirefoxScriptId(id)) {
    return RespondNow(NoArguments());
  }
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension_id());
  RemoveScripts(
      loader, {id},
      base::BindOnce(&ContentScriptsInternalUnregisterFunction::OnUnregistered,
                     this));
  return RespondLater();
}

void ContentScriptsInternalUnregisterFunction::OnUnregistered(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }
  Respond(NoArguments());
}

}  // namespace extensions
