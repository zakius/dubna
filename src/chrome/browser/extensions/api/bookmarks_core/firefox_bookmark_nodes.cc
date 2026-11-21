// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/bookmarks_core/firefox_bookmark_nodes.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/bookmarks_core/firefox_bookmark_ids.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

namespace {

// Gives the folder ID under `key` of `dict` Firefox's ID, if it has one.
void ToFirefoxId(const bookmarks::BookmarkModel& model,
                 base::DictValue& dict,
                 std::string_view key) {
  std::string* id = dict.FindString(key);
  if (id) {
    ToFirefoxBookmarkId(model, id);
  }
}

// Gives `node`, a node of the bookmarks API, and its children Firefox's type
// and IDs. Chromium gives a bookmark a URL and a folder none.
void ToFirefoxNode(const bookmarks::BookmarkModel& model,
                   base::DictValue& node) {
  node.Set("type", node.contains("url") ? "bookmark" : "folder");
  ToFirefoxId(model, node, "id");
  ToFirefoxId(model, node, "parentId");
  base::ListValue* children = node.FindList("children");
  if (!children) {
    return;
  }
  for (base::Value& child : *children) {
    ToFirefoxNode(model, child.GetDict());
  }
}

bool WillDispatchFirefoxBookmarkEvent(
    const std::string& event_name,
    const scoped_refptr<const EventArgs>& event_args,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::DictValue* listener_filter,
    std::optional<base::ListValue>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out,
    bool* dispatch_separate_event_out) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return true;
  }
  const bookmarks::BookmarkModel& model =
      *BookmarkModelFactory::GetForBrowserContext(browser_context);
  event_args_out = event_args->data.Clone();
  base::ListValue& args = *event_args_out;
  // Each event but onImportBegan and onImportEnded has the node's ID first,
  // then an object: the node for onCreated, and for the others its parent
  // IDs (onRemoved, onMoved), its node (onRemoved), or neither.
  if (args.size() < 2) {
    return true;
  }
  ToFirefoxBookmarkId(model, &args[0].GetString());
  base::DictValue& info = args[1].GetDict();
  if (event_name == api::bookmarks::OnCreated::kEventName) {
    ToFirefoxNode(model, info);
    return true;
  }
  ToFirefoxId(model, info, "parentId");
  ToFirefoxId(model, info, "oldParentId");
  base::DictValue* node = info.FindDict("node");
  if (!node) {
    return true;
  }
  ToFirefoxNode(model, *node);
  // onRemoved's node has left its parent, so Chromium gives it no parentId or
  // index; Firefox gives it where it was, as removeInfo has it.
  if (event_name == api::bookmarks::OnRemoved::kEventName) {
    const std::string* parent_id = info.FindString("parentId");
    if (parent_id) {
      node->Set("parentId", *parent_id);
    }
    std::optional<int> index = info.FindInt("index");
    if (index) {
      node->Set("index", *index);
    }
  }
  return true;
}

}  // namespace

base::ListValue FirefoxBookmarkResults(const Extension* extension,
                                       const bookmarks::BookmarkModel& model,
                                       base::ListValue results) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return results;
  }
  // A function answers one node (create(), move(), update()) or a list of
  // them (get(), getTree(), search() and the others).
  for (base::Value& result : results) {
    if (result.is_dict()) {
      ToFirefoxNode(model, result.GetDict());
    } else if (result.is_list()) {
      for (base::Value& node : result.GetList()) {
        ToFirefoxNode(model, node.GetDict());
      }
    }
  }
  return results;
}

Event::WillDispatchCallback FirefoxBookmarkEventCallback(
    const std::string& event_name,
    scoped_refptr<const EventArgs> event_args) {
  return base::BindRepeating(&WillDispatchFirefoxBookmarkEvent, event_name,
                             std::move(event_args));
}

bool ApplyFirefoxBookmarkType(const std::optional<std::string>& type,
                              std::string* url,
                              std::string* error) {
  if (!type) {
    return true;
  }
  if (*type == "folder") {
    url->clear();
    return true;
  }
  if (*type == "bookmark") {
    if (url->empty()) {
      *error = bookmarks_errors::kInvalidUrlError;
      return false;
    }
    return true;
  }
  *error = "Error at property 'type': Value must be one of bookmark, folder.";
  return false;
}

}  // namespace extensions
