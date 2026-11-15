// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/bookmarks_core/firefox_bookmark_ids.h"

#include "base/strings/string_number_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace extensions {

namespace {

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

}  // namespace

bool ParseBookmarkId(const bookmarks::BookmarkModel& model,
                     std::string_view id_string,
                     int64_t* id) {
  if (base::StringToInt64(id_string, id)) {
    return true;
  }
  const bookmarks::BookmarkNode* folder = FirefoxFolder(model, id_string);
  if (!folder) {
    return false;
  }
  *id = folder->id();
  return true;
}

}  // namespace extensions
