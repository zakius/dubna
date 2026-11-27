// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_CLICK_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_CLICK_H_

#include "base/auto_reset.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}

namespace ui {
class Event;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox gives the
// onClicked of browserAction, action and pageAction a second argument,
// OnClickData: `button`, 0 for a left click or a key press and 1 for a middle
// click, and `modifiers`, the keys held. A middle click on the action's button
// runs the action too: the add-on gets activeTab, and onClicked fires even
// when the action has a popup, which the add-on may open itself. Firefox for
// Android has neither, nor does tennessine.
//
// While it lives, an action that its button runs runs for `event`, the click
// or key press on the button: Chromium's toolbar button and Extensions menu
// row, and oganesson's page action button in the location bar. An action run
// outside one (its keyboard shortcut, the menus command, pageAction.openPopup)
// gets {button: 0, modifiers: []}, as Firefox's triggerAction() gives it.
class ScopedFirefoxActionClick {
 public:
  explicit ScopedFirefoxActionClick(const ui::Event& event);
  ScopedFirefoxActionClick(const ScopedFirefoxActionClick&) = delete;
  ScopedFirefoxActionClick& operator=(const ScopedFirefoxActionClick&) =
      delete;
  ~ScopedFirefoxActionClick();

 private:
  base::AutoReset<int> button_;
  base::AutoReset<int> event_flags_;
};

// Whether `event`, on the button of the action of `extension_id` in the
// window of `web_contents`, is a middle click that runs the action: the
// middle button's release, for a Firefox add-on.
bool IsFirefoxMiddleClick(const ui::Event& event,
                          content::WebContents* web_contents,
                          const ExtensionId& extension_id);

// Whether `extension`'s action runs for a middle click on its button, which
// fires onClicked in place of the popup.
bool IsFirefoxMiddleClickRunning(const Extension& extension);

// Adds OnClickData to `args`, the arguments of `extension`'s onClicked, for a
// Firefox add-on on the desktop.
void AddFirefoxOnClickData(const Extension* extension, base::ListValue& args);

// OnClickData's `modifiers` for the keys held in `event_flags`, in Firefox's
// order: Shift, Alt, Command, Ctrl, and MacCtrl with Ctrl on a Mac.
base::ListValue FirefoxClickModifiers(int event_flags);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_CLICK_H_
