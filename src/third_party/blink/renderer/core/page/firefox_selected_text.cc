// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "third_party/blink/renderer/core/page/firefox_selected_text.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

// Firefox's SelectionUtils passes up to 16K characters to add-ons.
constexpr wtf_size_t kMaxLength = 16384;

// The white space of JavaScript's trim(), ECMAScript's WhiteSpace and
// LineTerminator: also U+00A0 (&nbsp;), U+FEFF and Unicode's other spaces,
// which String::StripWhiteSpace() leaves out.
bool IsJavaScriptSpace(UChar c) {
  switch (c) {
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case 0x2028:
    case 0x2029:
    case 0xFEFF:
      return true;
    default:
      return unicode::Category(c) == unicode::kSeparator_Space;
  }
}

// Whether the selection starts at white space that the rendered text leaves
// out, such as the indent of a line.
bool StartsAtSpace(const LocalFrame& frame, const String& text) {
  if (IsHTMLSpace(text[0])) {
    return false;
  }
  const Position& start =
      frame.Selection().GetSelectionInDomTree().ComputeStartPosition();
  const auto* text_node = DynamicTo<Text>(start.ComputeContainerNode());
  if (!text_node) {
    return false;
  }
  wtf_size_t offset = start.ComputeOffsetInContainerNode();
  return offset < text_node->length() && IsHTMLSpace(text_node->data()[offset]);
}

}  // namespace

String FirefoxSelectedText(const LocalFrame& frame) {
  String text = frame.SelectedText(TextIteratorBehavior::Builder()
                                       .SetSkipsUnselectableContent(true)
                                       .Build());
  // White space alone is no selection to Firefox, which trims the text with
  // trim() to tell (SelectionUtils).
  if (text.StripWhiteSpace(IsJavaScriptSpace).empty()) {
    return String();
  }
  if (StartsAtSpace(frame, text)) {
    StringBuilder builder;
    builder.Append(' ');
    builder.Append(text);
    text = builder.ToString();
  }
  return text.substr(0, kMaxLength);
}

}  // namespace blink
