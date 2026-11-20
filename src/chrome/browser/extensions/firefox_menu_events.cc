// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_events.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/event_args.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

using ItemMap = std::map<int, MenuItem::Id>;
using MediaType = blink::mojom::ContextMenuDataMediaType;

// The open menu, as Firefox's gMenuBuilder has one.
FirefoxMenuState* g_open_menu = nullptr;

void Dispatch(content::BrowserContext* context,
              const ExtensionId& extension_id,
              std::string_view event_name,
              base::ListValue args) {
  EventRouter* router = EventRouter::Get(context);
  if (!router) {
    return;
  }
  router->DispatchEventToExtension(
      extension_id,
      std::make_unique<Event>(events::FOR_TEST, event_name,
                              base::MakeRefCounted<EventArgs>(std::move(args))));
}

// Firefox's canAccessContext(): a menu in a private window is only for the
// add-ons that may run there.
bool CanAccess(const Extension& extension, content::BrowserContext* context) {
  return IsFirefoxAddOn(extension) &&
         (!context->IsOffTheRecord() ||
          util::IsIncognitoEnabled(extension.id(), context));
}

// The Firefox add-ons that get onShown in a page's or a tab's menu: those
// that listen to it, also in an event page that has stopped, which it wakes.
std::vector<const Extension*> OnShownListeners(
    content::BrowserContext* context) {
  std::vector<const Extension*> listeners;
  EventRouter* router = EventRouter::Get(context);
  if (!router) {
    return listeners;
  }
  for (const auto& extension :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    if (CanAccess(*extension, context) &&
        router->ExtensionHasEventListener(
            extension->id(), api::context_menus::OnShown::kEventName)) {
      listeners.push_back(extension.get());
    }
  }
  return listeners;
}

// Firefox gives onShown the menu's URLs and text only for a page that the
// add-on may access: with its host permissions, activeTab on `tab`, or its
// own page.
bool CanSeePage(const Extension& extension,
                const GURL& url,
                content::WebContents* tab) {
  if (url::Origin::Create(url).IsSameOriginWith(extension.origin())) {
    return true;
  }
  int tab_id = tab ? ExtensionTabUtil::GetTabId(tab) : -1;
  return extension.permissions_data()->GetPageAccess(url, tab_id, nullptr) ==
         PermissionsData::PageAccess::kAllowed;
}

bool IsTab(content::WebContents* web_contents) {
  return GetViewType(web_contents) == mojom::ViewType::kTabContents;
}

// Firefox's viewType of the page: "tab", or an add-on's "popup" or
// "sidebar" (this browser's side panel).
const char* ViewType(content::WebContents* web_contents) {
  switch (GetViewType(web_contents)) {
    case mojom::ViewType::kTabContents:
      return "tab";
    case mojom::ViewType::kExtensionPopup:
      return "popup";
    case mojom::ViewType::kExtensionSidePanel:
      return "sidebar";
    default:
      return nullptr;
  }
}

// A link, also one whose URL isn't valid (href="http://"), which Firefox
// calls a link (firefox_menu_items.h).
bool OnLink(const content::ContextMenuParams& params) {
  return !params.unfiltered_link_url.is_empty() ||
         !params.invalid_link_url.empty();
}

std::string LinkUrl(const content::ContextMenuParams& params) {
  if (!params.unfiltered_link_url.is_empty()) {
    return params.unfiltered_link_url.spec();
  }
  return params.invalid_link_url;
}

// A link without text, such as one around an image, has its URL as its
// text in Firefox.
std::u16string LinkText(const content::ContextMenuParams& params) {
  if (base::TrimWhitespace(params.link_text, base::TRIM_ALL).empty()) {
    return base::UTF8ToUTF16(LinkUrl(params));
  }
  return params.link_text;
}

// The text selected in the frame, as onClicked gives it
// (AddFirefoxClickData()).
std::u16string SelectionText(const content::ContextMenuParams& params) {
  if (!params.firefox_selection_text.empty()) {
    return params.firefox_selection_text;
  }
  return params.selection_text;
}

// Firefox's getMenuContexts() for a page's menu, in its order.
base::ListValue PageContexts(const content::ContextMenuParams& params) {
  base::ListValue contexts;
  if (params.media_type == MediaType::kAudio) {
    contexts.Append("audio");
  }
  if (params.is_editable) {
    contexts.Append("editable");
  }
  if (params.is_subframe) {
    contexts.Append("frame");
  }
  if (params.media_type == MediaType::kImage) {
    contexts.Append("image");
  }
  if (OnLink(params)) {
    contexts.Append("link");
  }
  if (params.form_control_type ==
      blink::mojom::FormControlType::kInputPassword) {
    contexts.Append("password");
  }
  if (!SelectionText(params).empty()) {
    contexts.Append("selection");
  }
  if (params.media_type == MediaType::kVideo) {
    contexts.Append("video");
  }
  if (contexts.empty()) {
    contexts.Append("page");
  }
  contexts.Append("all");
  return contexts;
}

// Firefox's addMenuEventInfo() for a page's menu.
base::DictValue PageMenuInfo(const Extension& extension,
                             content::WebContents* web_contents,
                             content::RenderFrameHost* frame,
                             const content::ContextMenuParams& params) {
  base::DictValue info;
  info.Set("contexts", PageContexts(params));
  if (const char* view_type = ViewType(web_contents)) {
    info.Set("viewType", view_type);
  }
  bool on_media = true;
  switch (params.media_type) {
    case MediaType::kImage:
      info.Set("mediaType", "image");
      break;
    case MediaType::kVideo:
      info.Set("mediaType", "video");
      break;
    case MediaType::kAudio:
      info.Set("mediaType", "audio");
      break;
    default:
      on_media = false;
  }
  if (frame) {
    int frame_id = ExtensionApiFrameIdMap::GetFrameId(frame);
    if (frame_id != ExtensionApiFrameIdMap::kInvalidFrameId) {
      info.Set("frameId", frame_id);
    }
  }
  info.Set("editable", params.is_editable);

  const GURL& url = params.is_subframe ? params.frame_url : params.page_url;
  if (!CanSeePage(extension, url,
                  IsTab(web_contents) ? web_contents : nullptr)) {
    return info;
  }
  if (OnLink(params)) {
    info.Set("linkText", LinkText(params));
    info.Set("linkUrl", LinkUrl(params));
  }
  if (on_media) {
    info.Set("srcUrl", params.src_url.spec());
  }
  info.Set("pageUrl", params.page_url.spec());
  if (params.is_subframe) {
    info.Set("frameUrl", params.frame_url.spec());
  }
  std::u16string selection = SelectionText(params);
  if (!selection.empty()) {
    info.Set("selectionText", selection);
  }
  return info;
}

// onShown's info in a menu of a tab or of an add-on's toolbar button, whose
// only context is the menu's: the tab's page URL with the add-on's access.
base::DictValue BrowserMenuInfo(const Extension& extension,
                               std::string_view context,
                               content::WebContents* tab) {
  base::DictValue info;
  base::ListValue contexts;
  contexts.Append(context);
  // Firefox's "all" is the contexts of a page, and an action's.
  if (context != "tab") {
    contexts.Append("all");
  }
  info.Set("contexts", std::move(contexts));
  info.Set("editable", false);
  if (tab && CanSeePage(extension, tab->GetLastCommittedURL(), tab)) {
    info.Set("pageUrl", tab->GetLastCommittedURL().spec());
  }
  return info;
}

// The context of the menu of `extension`'s toolbar button.
const char* ActionContext(const Extension& extension,
                          content::BrowserContext* context) {
  ExtensionAction* action =
      ExtensionActionManager::Get(context)->GetExtensionAction(extension);
  if (action && action->action_type() == ActionInfo::Type::kPage) {
    return "page_action";
  }
  return extension.manifest_version() >= 3 ? "action" : "browser_action";
}

// Whether the menu shows the item `id`: it and its parents are visible.
bool IsShown(const MenuManager& manager, const MenuItem::Id& id) {
  const MenuItem* item = manager.GetItemById(id);
  while (item) {
    if (!item->visible()) {
      return false;
    }
    if (!item->parent_id()) {
      return true;
    }
    item = manager.GetItemById(*item->parent_id());
  }
  return false;
}

// onShown's menuIds: the IDs of the add-on's items that the menu shows,
// parents before their children, as Firefox lists them.
base::ListValue ShownMenuIds(const ItemMap& items,
                             const MenuManager& manager,
                             const ExtensionId& extension_id) {
  base::ListValue ids;
  for (const auto& [command_id, id] : items) {
    if (id.extension_key.extension_id != extension_id ||
        !IsShown(manager, id)) {
      continue;
    }
    if (id.uid == 0) {
      ids.Append(id.string_uid);
    } else {
      ids.Append(id.uid);
    }
  }
  return ids;
}

void SendOnShown(FirefoxMenuState& state,
                 const Extension& extension,
                 base::DictValue info,
                 const ItemMap& items,
                 content::WebContents* tab) {
  MenuManager* manager = MenuManager::Get(state.browser_context);
  if (!manager) {
    return;
  }
  info.Set("menuIds", ShownMenuIds(items, *manager, extension.id()));
  base::ListValue args;
  args.Append(std::move(info));
  if (tab) {
    ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(
            &extension, mojom::ContextType::kPrivilegedExtension, tab);
    args.Append(
        ExtensionTabUtil::CreateTabObject(tab, scrub_tab_behavior, &extension)
            .ToValue());
  }
  state.shown_to.insert(extension.id());
  Dispatch(state.browser_context, extension.id(),
           api::context_menus::OnShown::kEventName, std::move(args));
}

// onHidden also goes to the Firefox add-ons whose items the menu showed,
// without an onShown listener too, as in Firefox.
void AddAddOnsOfShownItems(FirefoxMenuState& state, const ItemMap& items) {
  MenuManager* manager = MenuManager::Get(state.browser_context);
  if (!manager) {
    return;
  }
  for (const auto& [command_id, id] : items) {
    const Extension* extension =
        ExtensionRegistry::Get(state.browser_context)
            ->enabled_extensions()
            .GetByID(id.extension_key.extension_id);
    if (extension && CanAccess(*extension, state.browser_context) &&
        IsShown(*manager, id)) {
      state.shown_to.insert(extension->id());
    }
  }
}

FirefoxMenuState& StateOf(std::unique_ptr<FirefoxMenuState>& state,
                          ContextMenuMatcher& matcher,
                          const ui::SimpleMenuModel& model) {
  if (!state) {
    // A menu that didn't say where its extension items are gets them at its
    // end.
    state = std::make_unique<FirefoxMenuState>(matcher);
    state->start = model.GetItemCount();
    state->end = state->start;
  }
  return *state;
}

// The add-on whose item, or whose submenu of items, is the menu's entry at
// `index`; empty for a separator or another entry.
ExtensionId EntryExtensionId(const ui::MenuModel& model,
                             size_t index,
                             const ItemMap& items) {
  int command_id = model.GetCommandIdAt(index);
  auto it = items.find(command_id);
  if (it != items.end()) {
    return it->second.extension_key.extension_id;
  }
  const ui::MenuModel* submenu = model.GetSubmenuModelAt(index);
  if (!submenu ||
      !ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return ExtensionId();
  }
  for (size_t i = 0; i < submenu->GetItemCount(); ++i) {
    ExtensionId extension_id = EntryExtensionId(*submenu, i, items);
    if (!extension_id.empty()) {
      return extension_id;
    }
  }
  return ExtensionId();
}

// The index that ContextMenuMatcher::AppendExtensionItems() takes for new
// items next to those in `model`: after the last one's command ID.
int NextExtensionIndex(const ui::MenuModel& model, const ItemMap& items) {
  int last = -1;
  for (const auto& [command_id, id] : items) {
    last = std::max(last, command_id);
  }
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    int command_id = model.GetCommandIdAt(i);
    if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
      last = std::max(last, command_id);
    }
  }
  if (last < 0) {
    return 0;
  }
  return last - ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0) + 1;
}

// Moves the entry at `from` to `to`, before it, with what the extension
// items have: their type, label, radio group, submenu and icon.
void MoveEntry(ui::SimpleMenuModel& model, size_t from, size_t to) {
  int command_id = model.GetCommandIdAt(from);
  std::u16string label = model.GetLabelAt(from);
  switch (model.GetTypeAt(from)) {
    case ui::MenuModel::TYPE_SEPARATOR:
      model.InsertSeparatorAt(to, model.GetSeparatorTypeAt(from));
      break;
    case ui::MenuModel::TYPE_CHECK:
      model.InsertCheckItemAt(to, command_id, label);
      break;
    case ui::MenuModel::TYPE_RADIO:
      model.InsertRadioItemAt(to, command_id, label,
                              model.GetGroupIdAt(from));
      break;
    case ui::MenuModel::TYPE_SUBMENU:
      model.InsertSubMenuAt(to, command_id, label,
                            model.GetSubmenuModelAt(from));
      break;
    default:
      model.InsertItemAt(to, command_id, label);
  }
  // The entry is now one further.
  ui::ImageModel icon = model.GetIconAt(from + 1);
  if (!icon.IsEmpty()) {
    model.SetIcon(to, icon);
  }
  model.RemoveItemAt(from + 1);
}

}  // namespace

FirefoxMenuState::FirefoxMenuState(ContextMenuMatcher& matcher)
    : matcher(&matcher) {}

FirefoxMenuState::~FirefoxMenuState() {
  Close();
}

void FirefoxMenuState::Open(content::BrowserContext* context) {
  // A menu shown again: its last showing ends.
  Close();
  open = true;
  browser_context = context;
  g_open_menu = this;
}

void FirefoxMenuState::Close() {
  if (!open) {
    return;
  }
  open = false;
  if (g_open_menu == this) {
    g_open_menu = nullptr;
  }
  std::set<ExtensionId> to = std::move(shown_to);
  shown_to.clear();
  // A menu destroyed with its profile has no one to tell.
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (!client || !client->IsValidContext(browser_context.get())) {
    return;
  }
  for (const ExtensionId& extension_id : to) {
    Dispatch(browser_context, extension_id,
             api::context_menus::OnHidden::kEventName, base::ListValue());
  }
}

void RefreshFirefoxMenu(content::BrowserContext* context,
                        const ExtensionId& extension_id) {
  if (!g_open_menu || !ExtensionsBrowserClient::Get()->IsSameContext(
                          context, g_open_menu->browser_context)) {
    return;
  }
  g_open_menu->matcher->FirefoxRefresh(extension_id);
}

void ContextMenuMatcher::FirefoxItemsAppended(
    size_t start,
    const std::u16string& selection_text,
    bool is_action_menu) {
  FirefoxMenuState& state = StateOf(firefox_menu_, *this, *menu_model_);
  state.start = start;
  state.end = menu_model_->GetItemCount();
  state.selection_text = selection_text;
  state.is_action_menu = is_action_menu;
}

void ContextMenuMatcher::FirefoxPageMenuShown(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame,
    const content::ContextMenuParams& params) {
  FirefoxMenuState& state = StateOf(firefox_menu_, *this, *menu_model_);
  state.Open(browser_context_);
  // The tab argument, which a page in an add-on's popup or side panel
  // doesn't have.
  content::WebContents* tab = IsTab(web_contents) ? web_contents : nullptr;
  for (const Extension* extension : OnShownListeners(browser_context_)) {
    SendOnShown(state, *extension,
                PageMenuInfo(*extension, web_contents, frame, params),
                extension_item_map_, tab);
  }
  AddAddOnsOfShownItems(state, extension_item_map_);
}

void ContextMenuMatcher::FirefoxActionMenuShown(
    const Extension& extension,
    content::WebContents* active_tab) {
  FirefoxMenuState& state = StateOf(firefox_menu_, *this, *menu_model_);
  state.action_extension_id = extension.id();
  state.Open(browser_context_);
  if (!CanAccess(extension, browser_context_)) {
    return;
  }
  SendOnShown(state, extension,
              BrowserMenuInfo(extension,
                             ActionContext(extension, browser_context_),
                             active_tab),
              extension_item_map_, active_tab);
}

void ContextMenuMatcher::FirefoxTabMenuShown(content::WebContents* tab) {
  FirefoxMenuState& state = StateOf(firefox_menu_, *this, *menu_model_);
  state.Open(browser_context_);
  for (const Extension* extension : OnShownListeners(browser_context_)) {
    SendOnShown(state, *extension, BrowserMenuInfo(*extension, "tab", tab),
                extension_item_map_, tab);
  }
  AddAddOnsOfShownItems(state, extension_item_map_);
}

void ContextMenuMatcher::FirefoxMenuHidden() {
  if (firefox_menu_) {
    firefox_menu_->Close();
  }
}

void ContextMenuMatcher::FirefoxRefresh(const ExtensionId& extension_id) {
  FirefoxMenuState& state = *firefox_menu_;
  if (state.is_action_menu && extension_id != state.action_extension_id) {
    return;
  }
  // Items after the extension items may have gone since.
  size_t end = std::min(state.end, menu_model_->GetItemCount());
  size_t start = std::min(state.start, end);

  // The add-on's entries go, and its new ones take the place of the first,
  // or come after the other add-ons'. The submenus of the old ones stay
  // until the matcher goes: the open menu's views may still have them.
  int index = 0;
  std::optional<size_t> first;
  if (state.is_action_menu) {
    // All of an action menu's extension items, and the separators around
    // them, are its add-on's.
    while (end > start) {
      menu_model_->RemoveItemAt(--end);
    }
    extension_item_map_.clear();
    first = start;
  } else {
    index = NextExtensionIndex(*menu_model_, extension_item_map_);
    for (size_t i = end; i > start; --i) {
      if (EntryExtensionId(*menu_model_, i - 1, extension_item_map_) ==
          extension_id) {
        menu_model_->RemoveItemAt(i - 1);
        --end;
        first = i - 1;
      }
    }
    std::erase_if(extension_item_map_, [&extension_id](const auto& entry) {
      return entry.second.extension_key.extension_id == extension_id;
    });
  }

  // AppendExtensionItems() appends to the model's end, from where they move.
  size_t appended_from = menu_model_->GetItemCount();
  AppendExtensionItems(MenuItem::ExtensionKey(extension_id),
                       state.selection_text, &index, state.is_action_menu);
  size_t appended = menu_model_->GetItemCount() - appended_from;
  size_t at = first.value_or(end);
  for (size_t i = 0; i < appended; ++i) {
    if (at + i != appended_from + i) {
      MoveEntry(*menu_model_, appended_from + i, at + i);
    }
  }
  state.start = start;
  state.end = end + appended;

  if (ui::MenuModelDelegate* delegate = menu_model_->menu_model_delegate()) {
    delegate->OnMenuStructureChanged();
  }
}

}  // namespace extensions
