// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "third_party/blink/renderer/core/html/document_input.h"

#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink::document_input {

namespace {

// Present on a window once its document received input. A new document has a
// new window, without it.
class DocumentInput final : public GarbageCollected<DocumentInput>,
                            public Supplement<LocalDOMWindow> {
 public:
  static constexpr char kSupplementName[] = "DubnaDocumentInput";
  explicit DocumentInput(LocalDOMWindow& window) : Supplement(window) {}
  void Trace(Visitor* visitor) const override { Supplement::Trace(visitor); }
};

}  // namespace

void DidReceiveUserActivation(
    Frame& frame,
    mojom::blink::UserActivationNotificationType type) {
  // An extension holds the activation of a click in a page for 5 s and passes
  // it on with the script it injects and the messages it sends: uBlock Origin
  // injects its scriptlets into the tab a middle-click opens with the click's
  // activation. A new app window's script runs with it too (kWebScriptExec).
  using Type = mojom::blink::UserActivationNotificationType;
  switch (type) {
    case Type::kExtensionEvent:
    case Type::kExtensionGuestView:
    case Type::kExtensionMessaging:
    case Type::kExtensionMessagingBothPrivileged:
    case Type::kExtensionMessagingSenderPrivileged:
    case Type::kExtensionMessagingReceiverPrivileged:
    case Type::kExtensionMessagingNeitherPrivileged:
    case Type::kWebScriptExec:
    case Type::kActorWebMCP:
      return;
    default:
      break;
  }
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  LocalDOMWindow* window = local_frame ? local_frame->DomWindow() : nullptr;
  if (window && !Supplement<LocalDOMWindow>::From<DocumentInput>(window)) {
    Supplement<LocalDOMWindow>::ProvideTo(
        *window, MakeGarbageCollected<DocumentInput>(*window));
  }
}

bool HadInput(const LocalFrame& frame) {
  return frame.DomWindow() &&
         Supplement<LocalDOMWindow>::From<DocumentInput>(frame.DomWindow());
}

bool HasTransientUserActivation(const LocalFrame& frame) {
  return LocalFrame::HasTransientUserActivation(&frame) && HadInput(frame);
}

}  // namespace blink::document_input
