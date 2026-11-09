# What the corpus misses in Chromium

Written by `tools/amo_corpus.py scan` from the 1000 add-ons of `corpus.json`. Each row is a manifest key, permission or `browser.*` API that add-ons use and Chromium doesn't have, with the number of add-ons that use it and the first 10 of them by users. The matching is approximate: APIs are found by name in the scripts, so a name in a comment or in code that never runs counts, an API reached another way (`const api = browser; api.menus`) does not, only Firefox's namespaces count, and a member counts as missing when its namespace's schema doesn't name it. Read an add-on before filing a step for it alone.

## Manifest keys

| Name | Add-ons | Which |
|---|---|---|
| `sidebar_action` | 45 | video-downloadhelper, bitwarden-password-manager, deepl-translate, tree-style-tab, styl-us, cookie-editor, single-file, phantom-app, simple-tab-groups, sidebery, and 35 more |
| `__gmxcom__default_locale` | 2 | webde-mailcheck, gmx-mailcheck |
| `__gmxcom__permissions` | 2 | webde-mailcheck, gmx-mailcheck |
| `-ms-preload` | 1 | ibm-aspera-connect |
| `//1` | 1 | uaswitcher |
| `//2` | 1 | uaswitcher |
| `container` | 1 | google-shortcuts-all-google-se |
| `data_collection_permissions` | 1 | ctct-dlp-extension |
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
| `menus` | 47 | ublock-origin, private-relay, read-aloud, downthemall, tree-style-tab, leechblock-ng, flagfox, single-file, save-webp-as-png-or-jpeg, adnauseam, and 37 more |
| `contextualIdentities` | 31 | tampermonkey, facebook-container, multi-account-containers, cookie-autodelete, tree-style-tab, sandvpn, new-tab-override, simple-tab-groups, sidebery, cookie-quick-manager, and 21 more |
| `browserSettings` | 16 | eset-browser-privacy-security, tree-style-tab, adaptive-tab-bar-colour, gesturefy, windscribe, new-tab-override, popup-blocker-ultimate, tab-stash, dark-background-light-text, new-tab-homepage, and 6 more |
| `theme` | 13 | darkreader, downthemall, tree-style-tab, firefox-color, adaptive-tab-bar-colour, turn-off-the-lights, sidebery, undoclosetabbutton, automatic-dark, foxytab, and 3 more |
| `tabHide` | 8 | tree-style-tab, simple-tab-groups, sidebery, tab-stash, groupspeeddial, foxytab, google-translate-elemen, tridactyl-vim |
| `webRequestFilterResponse` | 8 | easy-youtube-video-download, noscript, media-downloader-unleashed, ins-downloader, age-verification-bypass, telegram-downloader, rajiko, geo-spoof |
| `find` | 4 | foxy-gestures, text-link, glitterdrag, tridactyl-vim |
| `menus.overrideContext` | 3 | tree-style-tab, sidebery, multithreaded-download-manager |
| `trialML` | 2 | mobidevtools, youtube-livechat-flusher |
| `webRequestFilterResponse.serviceWorkerScript` | 2 | noscript, geo-spoof |
| `mozillaAddons` | 1 | firefox-translations |
| `pkcs11` | 1 | belgium-eid |

## APIs

| Name | Add-ons | Which |
|---|---|---|
| `runtime.getBrowserInfo` | 92 | adblock-plus, privacy-badger17, adblock-for-firefox, facebook-container, to-google-translate, tonec-idm-integration-module, malwarebytes, search_by_image, wikibuy-for-firefox, private-relay, and 82 more |
| `menus` | 39 | private-relay, tree-style-tab, leechblock-ng, flagfox, single-file, save-webp-as-png-or-jpeg, new-tab-override, simple-tab-groups, sidebery, undoclosetabbutton, and 29 more |
| `proxy.onRequest` | 31 | multi-account-containers, foxyproxy-standard, 1vpn, rutracker-add-on, surfshark-vpn-proxy, sandvpn, mozilla-vpn-extension, hoxx-vpn-proxy, windscribe, setupvpn, and 21 more |
| `commands.update` | 29 | darkreader, traduzir-paginas-web, multi-account-containers, 1password-x-password-manager, keepassxc-browser, styl-us, flagfox, expressvpn, sidebery, yomitan, and 19 more |
| `contextualIdentities` | 26 | facebook-container, multi-account-containers, noscript, cookie-autodelete, tree-style-tab, sandvpn, new-tab-override, simple-tab-groups, sidebery, cookie-quick-manager, and 16 more |
| `sidebarAction` | 24 | bitwarden-password-manager, deepl-translate, tree-style-tab, styl-us, sidebery, notes-by-firefox, side-view, websites-notes, tabby-window-tab-manager, whatsapp-in-sidebar, and 14 more |
| `contentScripts` | 22 | ublock-origin, bitwarden-password-manager, ghostery, betterttv, noscript, violentmonkey, btroblox, sandvpn, save-webp-as-png-or-jpeg, windscribe, and 12 more |
| `theme` | 21 | darkreader, traduzir-paginas-web, tree-style-tab, firefox-color, mozilla-vpn-extension, turn-off-the-lights, uaswitcher, sidebery, undoclosetabbutton, automatic-dark, and 11 more |
| `webRequest.filterResponseData` | 21 | ublock-origin, ghostery, noscript, styl-us, surfshark-vpn-proxy, windscribe, adnauseam, avira-browser-safety, media-downloader-unleashed, ins-downloader, and 11 more |
| `commands.reset` | 20 | multi-account-containers, keepassxc-browser, styl-us, flagfox, sidebery, yomitan, print-edit-we, save-page-we, zoom-page-we, whatsapp-in-sidebar, and 10 more |
| `browserAction.setBadgeTextColor` | 18 | privacy-badger17, clearurls, browsec, multi-account-containers, cookie-autodelete, startpage-privacy-protection, alerabat-kupony-kody-rabatowe, save-page-we, foxytab, localcdn-fork-of-decentraleyes, and 8 more |
| `proxy.onError` | 15 | rutracker-add-on, surfshark-vpn-proxy, windscribe, zenmate-free-vpn-best, zeroomega, private-internet-access-ext, обход-блокировок-рунета, vpn-____, proxtube, proxy-toggle, and 5 more |
| `tabs.captureTab` | 13 | easyscreenshot, noscript, tree-style-tab, single-file, simple-tab-groups, sidebery, 1c-enterprise-8-extension, groupspeeddial, foxytab, tabby-window-tab-manager, and 3 more |
| `browserSettings` | 12 | tree-style-tab, gesturefy, windscribe, new-tab-override, popup-blocker-ultimate, dark-background-light-text, new-tab-homepage, automatic-dark, strict-pop-up-blocker, zoom-page-we, and 2 more |
| `search.get` | 10 | startpage-private-search, gesturefy, startpage-privacy-protection, webde-mailcheck, gmx-mailcheck, yandex-search-engine, swift-selection-search, add-custom-search-engine, speed-test-meter-and-search, glitterdrag |
| `commands.openShortcutSettings` | 8 | foxyproxy-standard, keepassxc-browser, turn-off-the-lights, chrome-mask, downloader-instagram, copyfish-ocr-software, stylebot-web, foxyproxy-basic |
| `tabs.hide` | 8 | tree-style-tab, sidebery, tab-stash, groupspeeddial, foxytab, mal-sync, google-translate-elemen, tridactyl-vim |
| `sessions.getTabValue` | 7 | tree-style-tab, simple-tab-groups, sidebery, new-tab-suspender, google-translate-elemen, open-tabs-next-to-current, reloadmatic |
| `sessions.setTabValue` | 7 | tree-style-tab, simple-tab-groups, sidebery, new-tab-suspender, google-translate-elemen, open-tabs-next-to-current, reloadmatic |
| `proxy.register` | 6 | rutracker-add-on, доступ-к-рутрекеру, zeroomega, switchyomega, rpa, lostfilm-tv-официальный-плагин |
| `tabs.saveAsPDF` | 6 | gesturefy, tranquility-1, print-edit-we, foxytab, print-to-pdf-document, save-pdf |
| `pageAction.openPopup` | 4 | traduzir-paginas-web, canvasblocker, qr-code-address-bar, google-translate-elemen |
| `pageAction.setBadgeBackgroundColor` | 4 | easy-video-downloader, open-in-internet-explorer, soundcloud-mp3-downloader, dark-theme-for-google |
| `pageAction.setBadgeText` | 4 | easy-video-downloader, open-in-internet-explorer, soundcloud-mp3-downloader, dark-theme-for-google |
| `sessions.getWindowValue` | 4 | tree-style-tab, simple-tab-groups, sidebery, google-translate-elemen |
| `sessions.setWindowValue` | 4 | tree-style-tab, simple-tab-groups, sidebery, google-translate-elemen |
| `tabs.show` | 4 | tree-style-tab, sidebery, foxytab, tridactyl-vim |
| `proxy.unregister` | 3 | zeroomega, switchyomega, rpa |
| `tabs.print` | 3 | gesturefy, print-edit-we, foxytab |
| `tabs.printPreview` | 3 | gesturefy, print-edit-we, capture-print |
| `tabs.warmup` | 3 | tree-style-tab, sidebery, groupspeeddial |
| `browserAction.getUserSettings` | 2 | privacy-badger17, 1password-x-password-manager |
| `events.data` | 2 | browsec, tubernet |
| `find` | 2 | text-link, glitterdrag |
| `proxy.registerProxyScript` | 2 | zeroomega, switchyomega |
| `runtime.getFrameId` | 2 | tonec-idm-integration-module, icloud-passwords |
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
| `downloads.drag` | 1 | s3download-statusbar |
| `extension.checkPaging` | 1 | table-capture-firefox |
| `history.onTitleChanged` | 1 | sidebery |
| `i18n.getAcceptedLanguages` | 1 | facebook-container |
| `i18n.getExistingLocales` | 1 | etc2 |
| `notifications.onSholwSettings` | 1 | distill-web-monitor-ff |
| `notifications.onShown` | 1 | empty-cache-button |
| `permissions.contians` | 1 | easy-video-downloader |
| `pkcs11` | 1 | belgium-eid |
| `proxy.proxyInfo` | 1 | mozilla-vpn-extension |
| `runtime.addListener` | 1 | wot-safe-browsing-tool |
| `runtime.getBackgroundClient` | 1 | netflix-party-is-now-teleparty |
| `runtime.onPerformanceWarning` | 1 | dashlane |
| `runtime.onRequest` | 1 | imtranslator |
| `runtime.onSyncMessage` | 1 | noscript |
| `runtime.onUninstalled` | 1 | pricy |
| `runtime.sendRequest` | 1 | imtranslator |
| `runtime.sendSyncMessage` | 1 | noscript |
| `storage.onChange` | 1 | ie-view-we |
| `tabs.activate` | 1 | evernote-web-clipper |
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
| `windows.waitUntilHasWindows` | 1 | nordvpn-proxy-extension |
