// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_menu_icons.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/menus/simple_menu_model.h"

namespace extensions {

namespace {

// Firefox's IconDetails.getPreferredIcon(): the path for `size` pixels, else
// for twice that, else for the smallest size above it, else for the largest.
// Empty when `icons` has no path.
std::string PreferredIconPath(const base::DictValue& icons, int size) {
  if (const std::string* path = icons.FindString(base::NumberToString(size))) {
    return *path;
  }
  if (const std::string* path =
          icons.FindString(base::NumberToString(2 * size))) {
    return *path;
  }
  std::map<int, std::string> paths;
  for (const auto [key, value] : icons) {
    int key_size = 0;
    if (base::StringToInt(key, &key_size) && value.is_string()) {
      paths[key_size] = value.GetString();
    }
  }
  if (paths.empty()) {
    return std::string();
  }
  auto bigger = paths.upper_bound(size);
  if (bigger != paths.end()) {
    return bigger->second;
  }
  return paths.rbegin()->second;
}

void OnIconLoaded(content::BrowserContext* context,
                  const MenuItem::Id& id,
                  const base::DictValue& icons,
                  const gfx::Image& image) {
  MenuItem* item = MenuManager::Get(context)->GetItemById(id);
  // The item may have gone, or have other icons since.
  if (!item || item->firefox_icons() != icons) {
    return;
  }
  item->set_firefox_icon(image);
}

// Loads `item`'s image: at each scale, the path that Firefox picks for 16 px
// times the scale, as Firefox picks for the screen's, drawn at 16 x 16.
void LoadIcon(content::BrowserContext* context,
              const Extension& extension,
              const MenuItem& item) {
  std::vector<ImageLoader::ImageRepresentation> representations;
  for (ui::ResourceScaleFactor factor : ui::GetSupportedResourceScaleFactors()) {
    float scale = ui::GetScaleForResourceScaleFactor(factor);
    int pixels = static_cast<int>(gfx::kFaviconSize * scale);
    std::string path = PreferredIconPath(item.firefox_icons(), pixels);
    if (path.empty()) {
      continue;
    }
    representations.emplace_back(
        extension.GetResource(path),
        ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(pixels, pixels), scale);
  }
  if (representations.empty()) {
    return;
  }
  ImageLoader::Get(context)->LoadImagesAsync(
      &extension, representations,
      base::BindOnce(&OnIconLoaded, context, item.id(),
                     item.firefox_icons().Clone()));
}

void LoadIcons(content::BrowserContext* context,
               const Extension& extension,
               const MenuItem::OwnedList& items) {
  for (const std::unique_ptr<MenuItem>& item : items) {
    if (!item->firefox_icons().empty()) {
      LoadIcon(context, extension, *item);
    }
    LoadIcons(context, extension, item->children());
  }
}

}  // namespace

void SetFirefoxMenuItemIcons(content::BrowserContext* context,
                             const Extension& extension,
                             MenuItem& item,
                             const std::map<std::string, std::string>& icons) {
  if (!IsFirefoxAddOn(extension)) {
    return;
  }
  base::DictValue paths;
  for (const auto& [size, path] : icons) {
    paths.Set(size, path);
  }
  item.set_firefox_icons(std::move(paths));
  item.set_firefox_icon(gfx::Image());
  LoadIcon(context, extension, item);
}

void LoadFirefoxMenuIcons(content::BrowserContext* context,
                          const Extension& extension) {
  const MenuItem::OwnedList* items = MenuManager::Get(context)->MenuItems(
      MenuItem::ExtensionKey(extension.id()));
  if (items) {
    LoadIcons(context, extension, *items);
  }
}

void ShowFirefoxMenuItemIcon(const MenuItem& item,
                             int command_id,
                             ui::SimpleMenuModel& menu_model) {
  if (item.firefox_icon().IsEmpty()) {
    return;
  }
  // A separator has no entry.
  std::optional<size_t> index = menu_model.GetIndexOfCommandId(command_id);
  if (index) {
    menu_model.SetIcon(*index, ui::ImageModel::FromImage(item.firefox_icon()));
  }
}

bool FirefoxShowsAddOnMenuIcon(const Extension* extension) {
  return !extension || !IsFirefoxAddOn(*extension) ||
         !IconsInfo::GetIcons(extension).empty();
}

}  // namespace extensions
