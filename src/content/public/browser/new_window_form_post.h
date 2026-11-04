// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CONTENT_PUBLIC_BROWSER_NEW_WINDOW_FORM_POST_H_
#define CONTENT_PUBLIC_BROWSER_NEW_WINDOW_FORM_POST_H_

#include "content/common/content_export.h"

namespace content {

class WebContents;

// Whether `contents` is a new window a form opened with POST (target=_blank),
// which loads the form's POST once its delegate has added it; with an opener,
// the opener's renderer submits it. The delegate gets only the URL, so it
// can't tell otherwise.
CONTENT_EXPORT bool IsNewWindowForFormPost(WebContents* contents);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NEW_WINDOW_FORM_POST_H_
