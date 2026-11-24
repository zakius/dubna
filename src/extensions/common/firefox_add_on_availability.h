// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_ADD_ON_AVAILABILITY_H_
#define EXTENSIONS_COMMON_FIREFOX_ADD_ON_AVAILABILITY_H_

#include "extensions/common/features/feature.h"

namespace extensions::firefox_add_on_availability {

// Firefox add-ons (dubna's webextensions/README.md, "Firefox add-ons only"):
// the API features that only Firefox add-ons have (IsFirefoxAddOn()), such as
// the theme namespace. Each one's entry in an _api_features.json says
// "requires_delegated_availability_check": true, and this map gives it its
// check: the bindings, the browser's functions and its events then leave
// other extensions without it. A feature with the flag that the map doesn't
// name is no extension's.
Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap();

}  // namespace extensions::firefox_add_on_availability

#endif  // EXTENSIONS_COMMON_FIREFOX_ADD_ON_AVAILABILITY_H_
