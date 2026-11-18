// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_FIREFOX_MENU_MODEL_ADAPTER_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_FIREFOX_MENU_MODEL_ADAPTER_H_

#include "base/functional/callback.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

// Firefox add-ons (dubna's webextensions/README.md): a page's context menu in
// which a middle click runs an item that `middle_click_runs` names by its
// command ID, as a left or right click does: Firefox runs a menus item on a
// middle click, and its menus.onClicked tells the button. Chromium's own items
// and other extensions' keep Chromium's, which leaves the menu open.
class FirefoxMenuModelAdapter : public views::MenuModelAdapter {
 public:
  FirefoxMenuModelAdapter(ui::MenuModel* menu_model,
                          base::RepeatingCallback<bool(int)> middle_click_runs);
  ~FirefoxMenuModelAdapter() override;

  // views::MenuModelAdapter:
  bool IsTriggerableEvent(views::MenuItemView* source,
                          const ui::Event& e) override;

 private:
  base::RepeatingCallback<bool(int)> middle_click_runs_;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_FIREFOX_MENU_MODEL_ADAPTER_H_
