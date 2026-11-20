// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/firefox_document_id.h"

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_api_frame_id_map.h"

namespace extensions {

std::string FirefoxDocumentId(content::RenderFrameHost& caller,
                              const blink::FrameToken& frame_token) {
  int process_id = caller.GetProcess()->GetDeprecatedID();
  content::RenderFrameHost* frame = nullptr;
  if (frame_token.Is<blink::LocalFrameToken>()) {
    frame = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            process_id, frame_token.GetAs<blink::LocalFrameToken>()));
  } else {
    frame = content::RenderFrameHost::FromPlaceholderToken(
        process_id, frame_token.GetAs<blink::RemoteFrameToken>());
  }
  if (!frame) {
    return std::string();
  }
  return ExtensionApiFrameIdMap::GetDocumentId(frame).ToString();
}

}  // namespace extensions
