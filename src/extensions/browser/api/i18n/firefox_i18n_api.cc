// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/api/i18n/firefox_i18n_api.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "build/build_config.h"
#include "extensions/common/api/i18n.h"

#if BUILDFLAG(IS_WIN)
#include "base/i18n/win/preferred_languages.h"
#elif BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/strings/string_split.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/LocaleUtils_jni.h"
#else
#include <memory>

#include "base/environment.h"
#include "base/strings/cstring_view.h"
#endif

namespace extensions {

namespace {

// Adds `locale` to `languages` as a BCP 47 tag, if it is one.
void AddLanguage(std::string_view locale, std::vector<std::string>& languages) {
  if (std::optional<base::i18n::LanguageTag> tag =
          base::i18n::GetLanguageTagFromString(locale)) {
    languages.emplace_back(tag->tag_string());
  }
}

// The system's languages as Gecko's OS preferences read them
// (intl/locale/*/OSPreferences_*.cpp). Chromium's own UI language reads other
// sources, such as LANGUAGE on Linux, so it can't stand in for them.
std::vector<std::string> ReadSystemLanguages() {
  std::vector<std::string> languages;
#if BUILDFLAG(IS_WIN)
  // The user's preferred UI languages, which Firefox reads when Windows'
  // GlobalizationPreferences give none.
  for (const base::i18n::LanguageTag& tag :
       base::i18n::GetUserPreferredUILanguageList()) {
    AddLanguage(tag.tag_string(), languages);
  }
#elif BUILDFLAG(IS_ANDROID)
  // The system's locale list, which Chromium's LocaleUtils gives as BCP 47
  // tags separated by commas.
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string list = Java_LocaleUtils_getDefaultLocaleListString(env);
  for (std::string_view locale : base::SplitStringPiece(
           list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    AddLanguage(locale, languages);
  }
#else
  // On Linux, ICU's default locale, which Gecko leaves as ICU finds it: the
  // first of LC_ALL, LC_MESSAGES and LANG that is set, without its codeset
  // and modifier ("en_US.UTF-8" is en-US). Chromium sets ICU's default locale
  // to its UI language, so this reads the variables itself.
  static constexpr base::cstring_view kVariables[] = {"LC_ALL", "LC_MESSAGES",
                                                      "LANG"};
  std::unique_ptr<base::Environment> environment = base::Environment::Create();
  for (base::cstring_view name : kVariables) {
    std::optional<std::string> value = environment->GetVar(name);
    if (!value || value->empty()) {
      continue;
    }
    std::string_view locale = *value;
    locale = locale.substr(0, locale.find_first_of(".@"));
    // C and POSIX name no language (ICU's en_US_POSIX): they get the
    // fallback below.
    if (locale != "C" && locale != "POSIX") {
      AddLanguage(locale, languages);
    }
    break;
  }
#endif
  return languages;
}

}  // namespace

ExtensionFunction::ResponseAction
I18nGetPreferredSystemLanguagesFunction::Run() {
  std::vector<std::string> languages = ReadSystemLanguages();
  // Gecko's OS preferences give en-US when they read no language.
  if (languages.empty()) {
    languages.push_back("en-US");
  }
  return RespondNow(ArgumentList(
      api::i18n::GetPreferredSystemLanguages::Results::Create(languages)));
}

}  // namespace extensions
