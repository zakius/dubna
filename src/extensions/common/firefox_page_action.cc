// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_page_action.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

struct FirefoxPageActionData : public Extension::ManifestData {
  static constexpr char kManifestDataKey[] = "firefox_page_action";

  explicit FirefoxPageActionData(std::unique_ptr<ActionInfo> info)
      : action_info(std::move(info)) {}

  std::unique_ptr<ActionInfo> action_info;
};

struct FirefoxShowMatchesData : public Extension::ManifestData {
  static constexpr char kManifestDataKey[] = "firefox_show_matches";

  URLPatternSet show_matches;
  URLPatternSet hide_matches;
};

// The schemes of Firefox's match patterns. Its "*" is http, https, ws and
// wss, Chromium's http and https: a page has neither ws nor wss.
constexpr int kMatchSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS | URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_FTP;

// Reads the page action's `key`, a list of at least one match pattern, into
// `patterns`, with <all_urls> only if `all_urls`, as Firefox's schema has
// them (page_action.json). Returns false and sets `error` for another value.
bool ParseMatches(const base::DictValue& page_action,
                  const char* key,
                  bool all_urls,
                  URLPatternSet& patterns,
                  std::u16string* error) {
  const base::Value* value = page_action.Find(key);
  if (!value) {
    return true;
  }
  const base::ListValue* list = value->GetIfList();
  if (!list || list->empty()) {
    *error = base::UTF8ToUTF16(
        base::StrCat({"Invalid value for 'page_action.", key, "'."}));
    return false;
  }
  for (size_t i = 0; i < list->size(); ++i) {
    const std::string* text = (*list)[i].GetIfString();
    const bool is_all_urls = text && *text == URLPattern::kAllUrlsPattern;
    // Firefox's <all_urls> matches data: too.
    URLPattern pattern(is_all_urls ? kMatchSchemes | URLPattern::SCHEME_DATA
                                   : kMatchSchemes);
    if (!text || (is_all_urls && !all_urls) ||
        pattern.Parse(*text) != URLPattern::ParseResult::kSuccess) {
      *error = base::UTF8ToUTF16(
          base::StrCat({"Invalid value for 'page_action.", key, "[",
                        base::NumberToString(i), "]'."}));
      return false;
    }
    patterns.AddPattern(pattern);
  }
  return true;
}

bool ParseShowMatches(Extension* extension, std::u16string* error) {
  const base::DictValue* dict =
      extension->manifest()->available_values().FindDict(
          manifest_keys::kPageAction);
  if (!dict || !IsFirefoxAddOn(*extension)) {
    return true;
  }
  auto data = std::make_unique<FirefoxShowMatchesData>();
  if (!ParseMatches(*dict, "show_matches", /*all_urls=*/true,
                    data->show_matches, error) ||
      !ParseMatches(*dict, "hide_matches", /*all_urls=*/false,
                    data->hide_matches, error)) {
    return false;
  }
  if (!data->show_matches.is_empty()) {
    extension->SetManifestData(std::move(data));
  }
  return true;
}

bool ParseSecondPageAction(Extension* extension, std::u16string* error) {
  const base::DictValue* dict =
      extension->manifest()->available_values().FindDict(
          manifest_keys::kPageAction);
  if (!dict) {
    *error = base::ASCIIToUTF16(
        std::string_view(manifest_errors::kInvalidPageAction));
    return false;
  }
  std::vector<InstallWarning> install_warnings;
  std::unique_ptr<ActionInfo> info = ActionInfo::Load(
      extension, ActionInfo::Type::kPage, *dict, &install_warnings, error);
  extension->AddInstallWarnings(std::move(install_warnings));
  if (!info) {
    return false;
  }
  extension->SetManifestData(
      std::make_unique<FirefoxPageActionData>(std::move(info)));
  return true;
}

}  // namespace

bool HasFirefoxPageAction(const Extension& extension) {
  const Manifest* manifest = extension.manifest();
  if (!manifest->FindKey(manifest_keys::kPageAction) ||
      !IsFirefoxAddOn(extension)) {
    return false;
  }
  return extension.manifest_version() >= 3 ||
         manifest->FindKey(manifest_keys::kAction) ||
         manifest->FindKey(manifest_keys::kBrowserAction);
}

bool HasChromiumPageAction(const Extension& extension) {
  return extension.manifest()->FindKey(manifest_keys::kPageAction) &&
         extension.manifest_version() < 3 && !HasFirefoxPageAction(extension);
}

bool ParseFirefoxPageAction(Extension* extension, std::u16string* error) {
  if (HasFirefoxPageAction(*extension) &&
      !ParseSecondPageAction(extension, error)) {
    return false;
  }
  return ParseShowMatches(extension, error);
}

const ActionInfo* GetFirefoxPageActionInfo(const Extension& extension) {
  const auto* data = extension.GetManifestData<FirefoxPageActionData>();
  return data ? data->action_info.get() : nullptr;
}

bool HasFirefoxShowMatches(const Extension& extension) {
  return extension.GetManifestData<FirefoxShowMatchesData>() != nullptr;
}

std::optional<bool> MatchFirefoxShowMatches(const Extension& extension,
                                            const GURL& url) {
  const auto* data = extension.GetManifestData<FirefoxShowMatchesData>();
  if (!data) {
    return std::nullopt;
  }
  return data->show_matches.MatchesURL(url) &&
         !data->hide_matches.MatchesURL(url);
}

}  // namespace extensions
