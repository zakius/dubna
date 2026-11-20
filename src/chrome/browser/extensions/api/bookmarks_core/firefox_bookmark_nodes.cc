// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/bookmarks_core/firefox_bookmark_nodes.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

namespace {

// Gives `node`, a node of the bookmarks API, and its children Firefox's type.
// Chromium gives a bookmark a URL and a folder none.
void ToFirefoxNode(base::DictValue& node) {
  node.Set("type", node.contains("url") ? "bookmark" : "folder");
  base::ListValue* children = node.FindList("children");
  if (!children) {
    return;
  }
  for (base::Value& child : *children) {
    ToFirefoxNode(child.GetDict());
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
  event_args_out = event_args->data.Clone();
  base::ListValue& args = *event_args_out;
  // onCreated(id, node) and onRemoved(id, {parentId, index, node}).
  if (event_name == api::bookmarks::OnCreated::kEventName) {
    ToFirefoxNode(args[1].GetDict());
  } else if (event_name == api::bookmarks::OnRemoved::kEventName) {
    ToFirefoxNode(*args[1].GetDict().FindDict("node"));
  }
  return true;
}

}  // namespace

base::ListValue FirefoxBookmarkResults(const Extension* extension,
                                       base::ListValue results) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return results;
  }
  // A function answers one node (create(), move(), update()) or a list of
  // them (get(), getTree(), search() and the others).
  for (base::Value& result : results) {
    if (result.is_dict()) {
      ToFirefoxNode(result.GetDict());
    } else if (result.is_list()) {
      for (base::Value& node : result.GetList()) {
        ToFirefoxNode(node.GetDict());
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

bool ApplyFirefoxBookmarkType(api::bookmarks::BookmarkTreeNodeType type,
                              std::string* url,
                              std::string* error) {
  if (type == api::bookmarks::BookmarkTreeNodeType::kFolder) {
    url->clear();
  } else if (type == api::bookmarks::BookmarkTreeNodeType::kBookmark &&
             url->empty()) {
    *error = bookmarks_errors::kInvalidUrlError;
    return false;
  }
  return true;
}

}  // namespace extensions
