// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/bookmarks_core/firefox_bookmark_ids.h"

#include "base/strings/string_number_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

namespace {

// The IDs that the API gives, one per folder: "menu________" isn't there, as
// Other bookmarks answers it but is "unfiled_____".
constexpr std::string_view kFirefoxFolderIds[] = {
    "root________", "toolbar_____", "unfiled_____", "mobile______"};

const bookmarks::BookmarkNode* FirefoxFolder(
    const bookmarks::BookmarkModel& model,
    std::string_view id_string) {
  if (id_string == "root________") {
    return model.root_node();
  }
  if (id_string == "toolbar_____") {
    return model.account_bookmark_bar_node() ? model.account_bookmark_bar_node()
                                             : model.bookmark_bar_node();
  }
  if (id_string == "menu________" || id_string == "unfiled_____") {
    return model.account_other_node() ? model.account_other_node()
                                      : model.other_node();
  }
  if (id_string == "mobile______") {
    return model.account_mobile_node() ? model.account_mobile_node()
                                       : model.mobile_node();
  }
  return nullptr;
}

// Firefox's ID of the folder whose number is `id`, or an empty one.
std::string_view FirefoxFolderId(const bookmarks::BookmarkModel& model,
                                 int64_t id) {
  for (std::string_view firefox_id : kFirefoxFolderIds) {
    const bookmarks::BookmarkNode* folder = FirefoxFolder(model, firefox_id);
    if (folder && folder->id() == id) {
      return firefox_id;
    }
  }
  return {};
}

}  // namespace

bool ParseBookmarkId(const Extension* extension,
                     const bookmarks::BookmarkModel& model,
                     std::string_view id_string,
                     int64_t* id) {
  if (base::StringToInt64(id_string, id)) {
    if (extension && IsFirefoxAddOn(*extension) &&
        !FirefoxFolderId(model, *id).empty()) {
      // No node has the ID -1.
      *id = -1;
    }
    return true;
  }
  const bookmarks::BookmarkNode* folder = FirefoxFolder(model, id_string);
  if (!folder) {
    return false;
  }
  *id = folder->id();
  return true;
}

void ToFirefoxBookmarkId(const bookmarks::BookmarkModel& model,
                         std::string* id_string) {
  int64_t id;
  if (!base::StringToInt64(*id_string, &id)) {
    return;
  }
  std::string_view firefox_id = FirefoxFolderId(model, id);
  if (!firefox_id.empty()) {
    *id_string = firefox_id;
  }
}

}  // namespace extensions
