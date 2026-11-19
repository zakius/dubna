// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

// Firefox add-ons (dubna's webextensions/README.md): SVG action icons. Firefox
// draws an add-on's SVG icons at any size. The browser decodes only raster
// images, so a Firefox add-on's pages draw them here, at the size each one is
// for: the paths of setIcon() (set_icon.js), and, in the background page, the
// manifest's default icons (firefox_manifest_svg_icons.h).

const IsFirefoxAddOn = requireNative('setIcon').IsFirefoxAddOn;

// The width of an action icon at 1x, as ExtensionAction::ActionIconSize().
const ACTION_ICON_SIZE = 16;

// Whether `path` is an SVG file that this module draws.
function drawsSvg(path) {
  return typeof path == 'string' &&
      $String.toLowerCase($String.slice(path, -4)) == '.svg' &&
      IsFirefoxAddOn();
}

// Draws the SVG at `path` in a `size` px square, keeping its aspect ratio,
// and calls `callback` with the ImageData, or `failureCallback` with an
// error message, as set_icon.js loads other images.
function load(path, size, callback, failureCallback) {
  const img = new Image();
  img.onerror = function() {
    const message = 'Could not load action icon \'' + path + '\'.';
    console.error(message);
    failureCallback(message);
  };
  img.onload = function() {
    // An SVG with only a viewBox has its aspect ratio as natural size.
    const scale = size / Math.max(img.naturalWidth, img.naturalHeight, 1);
    const width = img.naturalWidth * scale;
    const height = img.naturalHeight * scale;
    const canvas = document.createElement('canvas');
    canvas.width = size;
    canvas.height = size;
    const context = canvas.getContext('2d');
    context.drawImage(
        img, (size - width) / 2, (size - height) / 2, width, height);
    callback(context.getImageData(0, 0, size, size));
  };
  img.src = path;
}

// Turns setIcon()'s path, when it is one SVG, into the 1x and 2x sizes, which
// load() draws. Chromium would draw it once, at its natural size.
function sizeSvgPath(details) {
  if (drawsSvg(details.path)) {
    const path = details.path;
    details.path = {__proto__: null};
    details.path[ACTION_ICON_SIZE] = path;
    details.path[2 * ACTION_ICON_SIZE] = path;
  }
}

// The path of `icons`, a manifest's icon or icons, that Chromium picks for
// `size`: the smallest one as large or larger, or else the largest.
function pickIcon(icons, size) {
  if (typeof icons == 'string') {
    return icons;
  }
  let best;
  let largest;
  for (const key in icons) {
    const keySize = +key;
    if (keySize >= size && (best == null || keySize < best)) {
      best = keySize;
    }
    if (largest == null || keySize > largest) {
      largest = keySize;
    }
  }
  const pick = best != null ? best : largest;
  return pick != null ? icons[pick] : undefined;
}

// In the background page: draws each action's default icon that is an SVG,
// at 1x and 2x, and sends it to the browser as the action's default icon,
// under any icon the add-on sets (FirefoxActionValues::SvgDefaultIcon()).
// Each start of the background page draws it again, so an update's new icon
// shows.
function drawManifestIcons() {
  const setIcon = require('setIcon').setIcon;
  const manifest = chrome.runtime.getManifest();
  const actions = {
    __proto__: null,
    action: 'action',
    browser_action: 'browserAction',
    page_action: 'pageAction',
  };
  for (const key in actions) {
    if (!manifest[key]) {
      continue;
    }
    // An action without an icon of its own shows the add-on's icons.
    const icons = manifest[key].default_icon || manifest.icons;
    if (!icons) {
      continue;
    }
    // The SVG that Chromium picks for 16 px, or else for 32 px, drawn at
    // both: the drawing stands in for the manifest's whole icon, a PNG of
    // the other size too.
    let path = pickIcon(icons, ACTION_ICON_SIZE);
    if (!drawsSvg(path)) {
      path = pickIcon(icons, 2 * ACTION_ICON_SIZE);
    }
    if (!drawsSvg(path)) {
      continue;
    }
    // Manifest paths start at the add-on's root, not at the page's folder.
    if (path[0] != '/') {
      path = '/' + path;
    }
    const details = {__proto__: null, path: {__proto__: null}};
    details.path[ACTION_ICON_SIZE] = path;
    details.path[2 * ACTION_ICON_SIZE] = path;
    const method = actions[key] + '.setIcon';
    setIcon(details, function(iconSpec) {
      // The browser keeps it apart from the icon the add-on sets, for a page
      // action too (firefox_svg_default_icon.h).
      iconSpec.firefoxSvgDefaultIcon = true;
      bindingUtil.sendRequest(
          method, [iconSpec, function() {}], /*options=*/ undefined);
    }, function() {});
  }
}

exports.$set('drawsSvg', drawsSvg);
exports.$set('load', load);
exports.$set('sizeSvgPath', sizeSvgPath);
exports.$set('drawManifestIcons', drawManifestIcons);
