// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DOCUMENT_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DOCUMENT_INPUT_H_

#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink-forward.h"

namespace blink {

class Frame;
class LocalFrame;

// Whether a document received the user's input, which a frame's user
// activation doesn't tell: the activation also comes from script that an
// extension runs or messages it sends, and from script Chromium runs with the
// activation of a click elsewhere. Since Chromium 158, a new window of an
// installed web app opened within 5 s of a click in another of its windows
// starts active that way. Chromium also keeps a frame's sticky activation, and
// a reused frame's transient one, across same-origin navigations.
namespace document_input {

// For Frame::NotifyUserActivationInFrame(): a transient user activation of
// `type` reached `frame`'s document, from input in it or in a child frame, or
// from another source.
void DidReceiveUserActivation(
    Frame& frame,
    mojom::blink::UserActivationNotificationType type);

// `frame`'s document received input. A new document starts without.
bool HadInput(const LocalFrame& frame);

// `frame`'s transient user activation, only once its document received input.
bool HasTransientUserActivation(const LocalFrame& frame);

}  // namespace document_input

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DOCUMENT_INPUT_H_
