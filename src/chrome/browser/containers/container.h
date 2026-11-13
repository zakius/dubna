// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_CONTAINERS_CONTAINER_H_
#define CHROME_BROWSER_CONTAINERS_CONTAINER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "third_party/skia/include/core/SkColor.h"

// Tab containers: separate cookies and site storage within one profile, as
// Firefox's Multi-Account Containers and Brave's containers have. Each
// container is a storage partition (containers_partition.h); a tab is in a
// container when its pages load in that partition.
namespace containers {

// The prefs, all per profile.
//
// The user's containers, in their order: a list of Container::ToDict().
inline constexpr char kContainersList[] = "containers.list";
// Containers whose storage may be on disk, by id: the user's that have been
// used, temporary ones, and deleted ones until their storage is gone. Each
// keeps its name, icon and color for the tabs still in it.
inline constexpr char kContainersUsed[] = "containers.used";
// The ids of containers that sync deleted while they had tabs or storage
// here, until the user chooses to delete them here too or keep them: a list.
inline constexpr char kContainersSyncDeleted[] = "containers.sync_deleted";
// Traffic Control's rules, in their order: a list of TrafficRule::ToDict().
inline constexpr char kTrafficControlRules[] = "containers.traffic_control";
// Where the browser's new tabs open: a container's id, "temporary" for a new
// temporary container each time, or empty, outside containers.
inline constexpr char kNewTabContainer[] = "containers.new_tab";

// A temporary container's id starts with this. It is never in the user's
// list, and its storage goes once no tab, saved session or closed tab uses it.
inline constexpr char kTemporaryIdPrefix[] = "t-";

struct Container {
  // What a partition name may hold: UUIDs, the default containers' names.
  static bool IsValidId(std::string_view id);
  // nullopt for a dict without a valid id or with a value of the wrong type.
  // A missing name, icon or color is empty.
  static std::optional<Container> FromDict(const base::DictValue& dict);

  base::DictValue ToDict() const;
  bool temporary() const;
  // The color to draw the container in.
  SkColor color_value() const;

  std::string id;
  std::string name;
  // Firefox's icon names: "fingerprint", "briefcase", "dollar", "cart",
  // "circle", "gift", "vacation", "food", "fruit", "pet", "tree", "chill",
  // "fence". A browser draws a name it has no icon for as a circle.
  std::string icon;
  // Firefox's color names: "blue", "turquoise", "green", "yellow", "orange",
  // "red", "pink", "purple", "toolbar". Another is drawn gray.
  std::string color;
};

// The containers of a list pref; entries that aren't valid are left out.
std::vector<Container> ContainersFromList(const base::ListValue& list);

}  // namespace containers

#endif  // CHROME_BROWSER_CONTAINERS_CONTAINER_H_
