// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/api/firefox_document_id.h"

#include <optional>
#include <string>
#include <utility>

#include "content/public/renderer/render_frame.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/firefox_document_ids.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"

namespace extensions {

namespace {

using RequestResult = APIBindingHooks::RequestResult;

// Firefox's errors (toolkit/components/extensions/child/ext-runtime.js).
constexpr char kInvalidTarget[] =
    "Invalid argument: target is not a valid window or frame element.";
constexpr char kNoDocument[] = "Could not determine document for target.";

RequestResult Throw(v8::Isolate* isolate, const char* message) {
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToV8(isolate, message)));
  return RequestResult(RequestResult::THROWN);
}

// Firefox takes these elements by their DOM class, which the tag gives.
bool IsFrameElement(const blink::WebElement& element) {
  return !element.IsNull() &&
         (element.HasHTMLTagName("iframe") || element.HasHTMLTagName("frame") ||
          element.HasHTMLTagName("embed") || element.HasHTMLTagName("object"));
}

}  // namespace

RequestResult HandleFirefoxGetDocumentId(
    ScriptContext* script_context,
    const APISignature* signature,
    const v8::LocalVector<v8::Value>& arguments,
    const APITypeReferenceMap& refs) {
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(script_context->v8_context(), arguments,
                                    refs);
  if (!parse_result.succeeded()) {
    return RequestResult(std::move(*parse_result.error));
  }

  v8::Isolate* isolate = script_context->isolate();
  // A worker has no windows or elements.
  if (script_context->IsForServiceWorker()) {
    return Throw(isolate, kInvalidTarget);
  }
  v8::Local<v8::Value> target = (*parse_result.arguments)[0];
  blink::WebFrame* frame = nullptr;
  if (std::optional<blink::WebFrame*> window_frame =
          blink::WebFrame::FromWindowOrNullopt(isolate, target)) {
    frame = *window_frame;
  } else if (blink::WebElement element =
                 blink::WebElement::FromV8Value(isolate, target);
             IsFrameElement(element)) {
    frame = blink::WebFrame::FromFrameOwnerElement(element);
  } else {
    return Throw(isolate, kInvalidTarget);
  }

  // The frame's token names it in this renderer, also when it is the
  // placeholder of a frame in another process, as a cross-site frame is.
  content::RenderFrame* render_frame = script_context->GetRenderFrame();
  ExtensionFrameHelper* frame_helper =
      render_frame ? ExtensionFrameHelper::Get(render_frame) : nullptr;
  std::string document_id;
  if (!frame || !frame_helper ||
      !frame_helper->GetLocalFrameHost()->GetDocumentId(frame->GetFrameToken(),
                                                        &document_id) ||
      document_id.empty()) {
    return Throw(isolate, kNoDocument);
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value =
      gin::StringToV8(isolate, DocumentIdInFirefoxForm(document_id));
  return result;
}

}  // namespace extensions
