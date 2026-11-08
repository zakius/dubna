// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/containers/container.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"

namespace containers {

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";
constexpr char kIcon[] = "icon";
constexpr char kColor[] = "color";

// Firefox's container colors.
constexpr auto kColors = base::MakeFixedFlatMap<std::string_view, SkColor>({
    {"blue", SkColorSetRGB(0x37, 0xad, 0xff)},
    {"turquoise", SkColorSetRGB(0x00, 0xc7, 0x9a)},
    {"green", SkColorSetRGB(0x51, 0xcd, 0x00)},
    {"yellow", SkColorSetRGB(0xff, 0xcb, 0x00)},
    {"orange", SkColorSetRGB(0xff, 0x9f, 0x00)},
    {"red", SkColorSetRGB(0xff, 0x61, 0x3d)},
    {"pink", SkColorSetRGB(0xff, 0x4b, 0xda)},
    {"purple", SkColorSetRGB(0xaf, 0x51, 0xf5)},
    {"toolbar", SkColorSetRGB(0x7c, 0x7c, 0x7d)},
});

}  // namespace

// static
bool Container::IsValidId(std::string_view id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  for (char c : id) {
    if (!base::IsAsciiAlphaNumeric(c) && c != '-' && c != '_') {
      return false;
    }
  }
  return true;
}

// static
std::optional<Container> Container::FromDict(const base::DictValue& dict) {
  const std::string* id = dict.FindString(kId);
  if (!id || !IsValidId(*id)) {
    return std::nullopt;
  }
  Container container{.id = *id};
  for (auto [key, field] : {std::pair(kName, &container.name),
                            std::pair(kIcon, &container.icon),
                            std::pair(kColor, &container.color)}) {
    const base::Value* value = dict.Find(key);
    if (value && !value->is_string()) {
      return std::nullopt;
    }
    if (value) {
      *field = value->GetString();
    }
  }
  return container;
}

base::DictValue Container::ToDict() const {
  return base::DictValue()
      .Set(kId, id)
      .Set(kName, name)
      .Set(kIcon, icon)
      .Set(kColor, color);
}

bool Container::temporary() const {
  return id.starts_with(kTemporaryIdPrefix);
}

SkColor Container::color_value() const {
  auto it = kColors.find(color);
  return it == kColors.end() ? kColors.at("toolbar") : it->second;
}

std::vector<Container> ContainersFromList(const base::ListValue& list) {
  std::vector<Container> containers;
  for (const base::Value& value : list) {
    if (!value.is_dict()) {
      continue;
    }
    std::optional<Container> container = Container::FromDict(value.GetDict());
    if (!container) {
      continue;
    }
    bool duplicate = false;
    for (const Container& other : containers) {
      duplicate |= other.id == container->id;
    }
    if (!duplicate) {
      containers.push_back(std::move(*container));
    }
  }
  return containers;
}

}  // namespace containers
