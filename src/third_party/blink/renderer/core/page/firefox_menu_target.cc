// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "third_party/blink/renderer/core/page/firefox_menu_target.h"

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

// A window's last menu target, as Firefox's ContextMenuChild keeps it in
// lastMenuTarget: weakly, so that the menu keeps no element alive.
class FirefoxLastMenuTarget final
    : public GarbageCollected<FirefoxLastMenuTarget>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit FirefoxLastMenuTarget(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element);
    Supplement<LocalDOMWindow>::Trace(visitor);
  }

  int id = 0;
  WeakMember<Element> element;
};

const char FirefoxLastMenuTarget::kSupplementName[] = "FirefoxLastMenuTarget";

// The element that Firefox's contextmenu event targets: the node itself, the
// element that a text node is in, or a document's root element.
Element* TargetElement(Node* node) {
  if (!node) {
    return nullptr;
  }
  if (auto* element = DynamicTo<Element>(node)) {
    return element;
  }
  if (auto* document = DynamicTo<Document>(node)) {
    return document->documentElement();
  }
  return node->ParentOrShadowHostElement();
}

}  // namespace

int SetFirefoxMenuTarget(Node* node) {
  Element* element = TargetElement(node);
  LocalDOMWindow* window =
      element ? element->GetDocument().domWindow() : nullptr;
  if (!window) {
    return 0;
  }
  auto* target =
      Supplement<LocalDOMWindow>::From<FirefoxLastMenuTarget>(window);
  if (!target) {
    target = MakeGarbageCollected<FirefoxLastMenuTarget>(*window);
    Supplement<LocalDOMWindow>::ProvideTo(*window, target);
  }
  // Unique in the renderer process, so that one window's ID never names
  // another's target. Firefox's is the contextmenu event's time stamp, which
  // only differs between the menus of one tab.
  static int last_id = 0;
  target->id = ++last_id;
  target->element = element;
  return target->id;
}

WebElement WebElement::FirefoxMenuTarget(WebLocalFrame& web_frame, int id) {
  // blink::To, since WebNode::To<T>() hides it in a WebElement member.
  LocalFrame* frame = blink::To<WebLocalFrameImpl>(web_frame).GetFrame();
  LocalDOMWindow* window = frame ? frame->DomWindow() : nullptr;
  if (!window || !id) {
    return WebElement();
  }
  auto* target =
      Supplement<LocalDOMWindow>::From<FirefoxLastMenuTarget>(window);
  if (!target || target->id != id) {
    return WebElement();
  }
  // Firefox's element.getRootNode({composed: true}) == the window's
  // document: connected, in a shadow tree too, and not moved to another
  // document.
  Element* element = target->element.Get();
  if (!element || !element->isConnected() ||
      &element->GetDocument() != window->document()) {
    return WebElement();
  }
  return WebElement(element);
}

}  // namespace blink
