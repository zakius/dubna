# What the corpus misses in Chromium

Written by `tools/amo_corpus.py scan` from the 836 add-ons of `corpus.json`. Each row is a manifest key, permission or `browser.*` API that add-ons use and Chromium doesn't have, with the number of add-ons that use it and the first 10 of them by users. The matching is approximate: APIs are found by name in the scripts, so a name in a comment or in code that never runs counts, an API reached another way (`const api = browser; api.menus`) does not, only Firefox's namespaces count, and a member counts as missing when its namespace's schema doesn't name it. Read an add-on before filing a step for it alone.

## Manifest keys

| Name | Add-ons | Which |
|---|---|---|
| `-ms-preload` | 1 | ibm-aspera-connect |
| `//1` | 1 | uaswitcher |
| `//2` | 1 | uaswitcher |
| `container` | 1 | google-shortcuts-all-google-se |
| `data_policy` | 1 | builtwith |
| `experiment_apis` | 1 | firefox-translations |
| `gecko` | 1 | sink-it-for-reddit |
| `minimum_opera_version` | 1 | besttimetracker |
| `permissions_notes` | 1 | google-translator-for-firefox |
| `privacy_policy` | 1 | danfeonline |
| `user_scripts` | 1 | tampermonkey |

## Permissions

| Name | Add-ons | Which |
|---|---|---|
| `menus` | 46 | ublock-origin, read-aloud, downthemall, tree-style-tab, leechblock-ng, flagfox, single-file, save-webp-as-png-or-jpeg, adnauseam, new-tab-override, and 36 more |
| `tabHide` | 8 | tree-style-tab, simple-tab-groups, sidebery, tab-stash, groupspeeddial, foxytab, google-translate-elemen, tridactyl-vim |
| `webRequestFilterResponse` | 7 | easy-youtube-video-download, noscript, media-downloader-unleashed, ins-downloader, age-verification-bypass, rajiko, geo-spoof |
| `find` | 4 | foxy-gestures, text-link, glitterdrag, tridactyl-vim |
| `menus.overrideContext` | 3 | tree-style-tab, sidebery, multithreaded-download-manager |
| `trialML` | 2 | mobidevtools, youtube-livechat-flusher |
| `webRequestFilterResponse.serviceWorkerScript` | 2 | noscript, geo-spoof |
| `mozillaAddons` | 1 | firefox-translations |
| `pkcs11` | 1 | belgium-eid |

## APIs

| Name | Add-ons | Which |
|---|---|---|
| `commands.update` | 21 | darkreader, traduzir-paginas-web, multi-account-containers, keepassxc-browser, styl-us, flagfox, sidebery, yomitan, print-edit-we, measure-it, and 11 more |
| `webRequest.filterResponseData` | 19 | ublock-origin, ghostery, noscript, styl-us, adnauseam, avira-browser-safety, media-downloader-unleashed, ins-downloader, age-verification-bypass, localcdn-fork-of-decentraleyes, and 9 more |
| `sidebarAction` | 18 | deepl-translate, tree-style-tab, styl-us, sidebery, notes-by-firefox, side-view, websites-notes, tabby-window-tab-manager, clippings, colorfultabs, and 8 more |
| `commands.reset` | 14 | multi-account-containers, keepassxc-browser, styl-us, flagfox, sidebery, yomitan, print-edit-we, save-page-we, zoom-page-we, tile-tabs-we, and 4 more |
| `tabs.captureTab` | 13 | easyscreenshot, noscript, tree-style-tab, single-file, simple-tab-groups, sidebery, 1c-enterprise-8-extension, groupspeeddial, foxytab, tabby-window-tab-manager, and 3 more |
| `commands.openShortcutSettings` | 8 | foxyproxy-standard, keepassxc-browser, turn-off-the-lights, chrome-mask, downloader-instagram, copyfish-ocr-software, stylebot-web, foxyproxy-basic |
| `search.get` | 8 | startpage-private-search, gesturefy, startpage-privacy-protection, yandex-search-engine, swift-selection-search, add-custom-search-engine, speed-test-meter-and-search, glitterdrag |
| `sessions.getTabValue` | 7 | tree-style-tab, simple-tab-groups, sidebery, new-tab-suspender, google-translate-elemen, open-tabs-next-to-current, reloadmatic |
| `sessions.setTabValue` | 7 | tree-style-tab, simple-tab-groups, sidebery, new-tab-suspender, google-translate-elemen, open-tabs-next-to-current, reloadmatic |
| `tabs.hide` | 7 | tree-style-tab, sidebery, tab-stash, groupspeeddial, foxytab, google-translate-elemen, tridactyl-vim |
| `proxy.register` | 5 | rutracker-add-on, доступ-к-рутрекеру, zeroomega, rpa, lostfilm-tv-официальный-плагин |
| `tabs.saveAsPDF` | 6 | gesturefy, tranquility-1, print-edit-we, foxytab, print-to-pdf-document, save-pdf |
| `pageAction.openPopup` | 4 | traduzir-paginas-web, canvasblocker, qr-code-address-bar, google-translate-elemen |
| `pageAction.setBadgeBackgroundColor` | 4 | easy-video-downloader, open-in-internet-explorer, soundcloud-mp3-downloader, dark-theme-for-google |
| `pageAction.setBadgeText` | 4 | easy-video-downloader, open-in-internet-explorer, soundcloud-mp3-downloader, dark-theme-for-google |
| `sessions.getWindowValue` | 4 | tree-style-tab, simple-tab-groups, sidebery, google-translate-elemen |
| `sessions.setWindowValue` | 4 | tree-style-tab, simple-tab-groups, sidebery, google-translate-elemen |
| `tabs.show` | 4 | tree-style-tab, sidebery, foxytab, tridactyl-vim |
| `proxy.unregister` | 2 | zeroomega, rpa |
| `tabs.print` | 3 | gesturefy, print-edit-we, foxytab |
| `tabs.printPreview` | 3 | gesturefy, print-edit-we, capture-print |
| `tabs.warmup` | 3 | tree-style-tab, sidebery, groupspeeddial |
| `contextualIdentities.getSupportedColors` | 2 | multi-account-containers, tree-style-tab |
| `contextualIdentities.getSupportedIcons` | 2 | multi-account-containers, tree-style-tab |
| `events.data` | 2 | browsec, tubernet |
| `find` | 2 | text-link, glitterdrag |
| `proxy.registerProxyScript` | 1 | zeroomega |
| `runtime.setUninstallUrl` | 2 | yandexbar, yandex-homepage |
| `sessions.forgetClosedTab` | 2 | new-tab-override, undoclosetabbutton |
| `sessions.removeTabValue` | 2 | tree-style-tab, simple-tab-groups |
| `sessions.removeWindowValue` | 2 | tree-style-tab, simple-tab-groups |
| `tabs.insertAfterCurrent` | 2 | tree-style-tab, gesturefy |
| `tabs.insertRelatedAfterCurrent` | 2 | tree-style-tab, gesturefy |
| `tabs.moveInSuccession` | 2 | tree-style-tab, sidebery |
| `tabs.queryAsync` | 2 | wikibuy-for-firefox, distill-web-monitor-ff |
| `tabs.toggleReaderMode` | 2 | gesturefy, tridactyl-vim |
| `trial` | 2 | mobidevtools, youtube-livechat-flusher |
| `contextualIdentities.onSiteAssociationChanged` | 1 | multi-account-containers |
| `contextualIdentities.querySiteAssociations` | 1 | multi-account-containers |
| `contextualIdentities.removeSiteAssociation` | 1 | multi-account-containers |
| `contextualIdentities.setSiteAssociation` | 1 | multi-account-containers |
| `downloads.drag` | 1 | s3download-statusbar |
| `extension.checkPaging` | 1 | table-capture-firefox |
| `history.onTitleChanged` | 1 | sidebery |
| `i18n.getAcceptedLanguages` | 1 | facebook-container |
| `i18n.getExistingLocales` | 1 | etc2 |
| `notifications.onSholwSettings` | 1 | distill-web-monitor-ff |
| `notifications.onShown` | 1 | empty-cache-button |
| `permissions.contians` | 1 | easy-video-downloader |
| `pkcs11` | 1 | belgium-eid |
| `runtime.addListener` | 1 | wot-safe-browsing-tool |
| `runtime.getFrameId` | 1 | tonec-idm-integration-module |
| `runtime.onRequest` | 1 | imtranslator |
| `runtime.onSyncMessage` | 1 | noscript |
| `runtime.onUninstalled` | 1 | pricy |
| `runtime.sendRequest` | 1 | imtranslator |
| `runtime.sendSyncMessage` | 1 | noscript |
| `storage.onChange` | 1 | ie-view-we |
| `tabs.attachToolsPanel` | 1 | distill-web-monitor-ff |
| `tabs.closeWindowWithLastTab` | 1 | tree-style-tab |
| `tabs.createAsync` | 1 | distill-web-monitor-ff |
| `tabs.dragDrop` | 1 | tree-style-tab |
| `tabs.fadeOutExplicitlyUnloadedTabs` | 1 | tree-style-tab |
| `tabs.fadeOutUnloadedTabs` | 1 | tree-style-tab |
| `tabs.opentabfor` | 1 | tab-mix-plus-webextension |
| `tabs.removeAsync` | 1 | distill-web-monitor-ff |
| `tabs.searchclipboardfor` | 1 | tree-style-tab |
| `tabs.selectOwnerOnClose` | 1 | tree-style-tab |
| `tabs.sendMessageEx` | 1 | citavi-picker |
| `tabs.toggleToolsPanel` | 1 | distill-web-monitor-ff |
| `tabs.updateAsync` | 1 | distill-web-monitor-ff |
| `webRequest.onNavigationError` | 1 | checkmarks-web-ext |
| `windows.createAsync` | 1 | distill-web-monitor-ff |
| `windows.onUpdated` | 1 | tile-tabs-we |
