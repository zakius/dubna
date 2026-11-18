// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "components/renderer_context_menu/views/firefox_menu_model_adapter.h"

#include <utility>

#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"

FirefoxMenuModelAdapter::FirefoxMenuModelAdapter(
    ui::MenuModel* menu_model,
    base::RepeatingCallback<bool(int)> middle_click_runs)
    : views::MenuModelAdapter(menu_model),
      middle_click_runs_(std::move(middle_click_runs)) {}

FirefoxMenuModelAdapter::~FirefoxMenuModelAdapter() = default;

bool FirefoxMenuModelAdapter::IsTriggerableEvent(views::MenuItemView* source,
                                                 const ui::Event& e) {
  if (views::MenuModelAdapter::IsTriggerableEvent(source, e)) {
    return true;
  }
  return middle_click_runs_ && e.IsMouseEvent() &&
         (e.flags() & ui::EF_MIDDLE_MOUSE_BUTTON) &&
         source->GetType() != views::MenuItemView::Type::kSubMenu &&
         middle_click_runs_.Run(source->GetCommand());
}
