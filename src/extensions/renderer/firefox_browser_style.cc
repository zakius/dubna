// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_browser_style.h"

#include <string>

#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/grit/extensions_renderer_generated_resources.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool IsPopup(blink::WebLocalFrame* frame) {
  if (frame->Parent()) {
    return false;
  }
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  ExtensionFrameHelper* helper =
      render_frame ? ExtensionFrameHelper::Get(render_frame) : nullptr;
  return helper && helper->view_type() == mojom::ViewType::kExtensionPopup;
}

bool HasBrowserStyle(const Extension& extension, const GURL& document_url) {
  const base::DictValue* manifest = extension.manifest()->value();
  const base::DictValue* action = manifest->FindDict("browser_action");
  const base::DictValue* page_action = manifest->FindDict("page_action");
  if (page_action) {
    const std::string* popup = page_action->FindString("default_popup");
    if (!action ||
        (popup && extension.ResolveExtensionURL(*popup) == document_url)) {
      action = page_action;
    }
  }
  return action && action->FindBool("browser_style").value_or(false);
}

}  // namespace

void InsertFirefoxBrowserStyle(blink::WebLocalFrame* frame,
                               const Extension* extension,
                               const GURL& document_url) {
  if (!extension || !IsFirefoxAddOn(*extension) ||
      extension->manifest_version() >= 3 || !IsPopup(frame) ||
      !HasBrowserStyle(*extension, document_url)) {
    return;
  }
  std::string sheet =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_EXTENSIONS_RENDERER_GENERATED_FIREFOX_EXTENSION_CSS);
  frame->GetDocument().InsertStyleSheet(blink::WebString::FromUtf8(sheet));
}

}  // namespace extensions
