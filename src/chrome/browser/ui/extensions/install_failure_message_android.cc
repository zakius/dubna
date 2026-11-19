// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/ui/extensions/install_failure_message_android.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace extensions {

namespace {

void DeleteMessage(messages::MessageWrapper* message,
                   messages::DismissReason reason) {
  delete message;
}

}  // namespace

content::WebContents* GetVisibleTab(Profile* profile) {
  content::WebContents* found = nullptr;
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile) {
      continue;
    }
    content::WebContents* active = tab_model->GetActiveWebContents();
    if (active && (tab_model->IsActiveModel() || !found)) {
      found = active;
    }
  }
  return found;
}

void ShowInstallFailureMessage(Profile* profile, const std::u16string& error) {
  content::WebContents* web_contents = GetVisibleTab(profile);
  if (!web_contents) {
    return;
  }
  // An identifier the browser doesn't use otherwise: it only names the
  // message in metrics.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::EXTENSIONS_REQUEST_ACCESS);
  message->SetTitle(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_ERROR_ALERT_HEADING));
  message->SetDescription(error);
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_APP_OK));
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_PERMISSION_WARNING));
  message->SetDismissCallback(
      base::BindOnce(&DeleteMessage, base::Unretained(message.get())));
  bool enqueued = messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
  if (enqueued) {
    // The dismiss callback deletes it.
    std::ignore = message.release();
  }
}

}  // namespace extensions
