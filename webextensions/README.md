# Firefox WebExtensions climb

## Goal

Firefox add-ons run in oganesson and tennessine as they run in Firefox. The goal has two measures:

1. The 1000 add-ons with the most users on addons.mozilla.org (AMO) install and work.
2. Firefox's own WebExtensions tests pass, after we port them to our test harness.

We get there in small steps (the "climb"). Each step is one API, one install flow, one behaviour or one improvement to the tools. Each step is released, so each step is useful alone.

## Where the code goes

- Shared code goes in dubna: new files in `src/` and small patches in `patches/`, as for the other shared features. Both browsers build them.
- Code goes in one browser only when there is a reason: UI that only that browser has (a desktop side panel, the location bar), or a patch that doesn't apply on the other browser's tree. Titanium (tennessine) patches the extensions code a lot, so check each shared patch on both trees.
- The test harness and the corpus runner are in oganesson's `tools/e2e/`, because they drive the desktop browser. The corpus list, the static scan and the score page are in dubna, because they don't depend on a browser.
- A download the browser makes by itself for Firefox add-ons (the AMO update check, the update's `.xpi`) follows browser traffic's own row for Firefox add-ons, `firefox_extensions`, apart from Chrome Web Store extensions. The bundled uBlock Origin is neither: its code ships with the browser, and its filter lists have their own row.
- Desktop comes first. Each step says what tennessine gets: now, in a later step, or never (with the reason).

## The old work

The WebExtensions work from Chromium 154 is archived in `~/dev/oganesson-webextensions` (`main/patches/core/firefox-*.patch`, `main/patches/ui/firefox-*.patch`, and the status table in `main/docs/firefox-webextensions.md`). Use it as a reference only. Don't apply it as it is. Each archived patch is a climb step: rewrite it to the current rules (new files, small hooks, no edits to an upstream's patches), on the current Chromium, with a test.

## How we measure

### Corpus score

`corpus.json` lists the top 1000 add-ons on AMO by users: the Gecko ID, the version and the SHA-256 of the `.xpi`. The list is fixed. Refresh it once a quarter, in its own commit, so a score change never comes from a list change. The `.xpi` files are cached outside the repo.

Each add-on gets the highest level it passes:

| Level | Check |
|---|---|
| L0 | The manifest loads. |
| L1 | The add-on installs. |
| L2 | The background starts, and no error names a missing API or manifest key. |
| L3 | A smoke check passes: the popup opens, the content scripts run. |
| L4 | A hand-written test for this add-on passes (Pouch, uBlock Origin, Dark Reader, Sidebery and others we pick). |

The corpus runner (oganesson's `tools/e2e/corpus.py`, whose docstring says how it checks each level) writes the count for each level to `score/<browser>.json`, with the date and the browser version. The score page on dubna's site shows the counts and their history.

### Test score

`firefox-tests.json` lists the WebExtensions test files in Firefox's source (`toolkit/components/extensions/test/` and `browser/components/extensions/test/`). Each file has one of these states:

- `ported`: it runs in our harness. The score counts how many of these pass.
- `todo`: not ported yet.
- `n/a`: it tests a Gecko internal, not WebExtensions behaviour. Each `n/a` has a one-line reason. Without this list, 100% can't be reached.

Port tests together with the API they test, not all at once.

### Choosing the next step

The static scan (`tools/amo_corpus.py scan`) reads every add-on in the corpus. It lists the manifest keys and the `browser.*` calls that Chromium doesn't have, and the number of add-ons each one blocks. The next step is the one that unblocks the most add-ons for its size. This is the hill-climb rule: the order isn't fixed in advance. Check it again after each step.

## Steps

Each step is a GitHub issue with the `climb` label and a size label (`size: nit`, `size: minor` or `size: medium`), in the repo where its code goes. Find them all with:

```sh
gh search issues --owner zakius --label climb --state open
```

A step body says:

- what it unlocks: which add-ons, which tests, or which later steps;
- its archive source, if it has one;
- what tennessine gets;
- `Depends on #N` for each step that must ship first;
- how to check that it is done.

A step must fit into one evening run. It may continue into a second run, but never into a third. A step that is too large for this is split before work starts. Tool work counts as a step: an improvement to the harness that a later step needs soon is enough for one evening. Build only the tools that the next few steps use.

A larger goal, for example "Sidebery works", is a group of steps. Name the goal in each of its steps.

### Filing new steps

The tool steps find most of the work, so they file many new steps. That is expected. The static scan, the test inventory and each corpus run file steps for what they find:

- one step for each missing API or manifest key, even when only one add-on uses it; the ranking sets the order, not the filing;
- tests that wait for an API go into that API's step; a step only for porting tests is filed when the API already works;
- a new cause of a failure in a corpus run (an error that is not yet in a step) gets its own step.

Before filing, search open and closed `climb` issues. If one already covers the item, add to it instead. Group very small items of one namespace into one step. Give each new step a size label and its `Depends on` lines.

## When a step runs

The evening task (dubna `README.md`, "Update policy") does a batch of climb steps when:

- no update was due, and
- the tracker batch was empty, or it was small enough that the climb batch can still be released by 06:00. The agent decides.

The 06:00 is the first one after the run starts: a run started early in the day, by hand or on schedule, works on until then. Builds run at any hour; during the day the system gives them fewer CPUs.

The batch is two or three `size: nit` or `size: minor` steps, or one `size: medium` step alone. With less time left, it takes fewer. It picks the open `climb` steps with the best gain for their size whose `Depends on` steps are closed, regressions first ("Keeping what works"). It doesn't take a `blocked` step. Each step is its own commit and its own row in the status table, but the batch shares one build, one corpus run of the affected add-ons and one release per browser. A step that breaks the build or the tests without a small fix is dropped from the batch, and the rest ship.

If a step isn't released by about 06:00, the agent releases the steps of the batch that are done, stops the rest, pushes each to a branch `climb-<repo>-<issue>`, and writes in the issue what is done and what is left. The next run continues it. If the second run can't finish it either, the agent releases the part that works (if it is useful alone), files the rest as new steps, and closes the issue.

When the last full corpus run is more than 7 days old, or ran on an older oganesson version than main's, the climb work for that evening is a full corpus run and a score update instead of a batch.

## Keeping what works

Each Chromium update can break a step that already shipped: a patch hunk moves, an API changes shape, MV2 loses more code. Shipped steps stay checked:

- Each step's end-to-end test stays in oganesson's `tools/e2e/`, so every update's test run covers it.
- Each oganesson update that is not a zero-day runs the full corpus after its build. tennessine shares the patch, so its patch check catches a moved hunk. A zero-day update ships without it, and the next climb run does the full corpus run first.
- An add-on that drops a level on an update doesn't hold the update back. If the fix is small and clear, the update stage fixes it. Otherwise it files a `climb` step for the regression ("Firefox add-ons: <what> broke on Chromium <N>"), with the add-ons it drops and the step that added the code, and records the lower score with the reason. The next climb batch takes regressions before new steps.

## Done

A step is done when:

1. It builds, and the patch check applies every patch with zero fuzz on each browser that gets the step.
2. It has an end-to-end test, or a ported Firefox test, for the new behaviour.
3. The corpus run of the add-ons it affects passes at the same level or higher. The score never goes down: a drop blocks the release, unless the step explains why and files a step to fix it. A Pouch regression always blocks the release.
4. The status table below is updated.
5. It is released: the browsers move their dubna submodule, merge to main and tag a release (README "Releases"), or (tool work) the tool or the score page is merged and, for the site, tagged. The release notes give the score change, for example "L2: 612 → 640".

A step that fails twice gets the `blocked` label and a comment that says why. Then take the next step.

## Risks

- **Manifest V2.** Most top add-ons are MV2. oganesson keeps MV2 with ungoogled-chromium's `extensions-manifestv2.patch`, tennessine with Titanium's `patch.sh`. Chromium removes more MV2 code in each milestone. Check both at each milestone. If MV2 goes, the corpus score falls a lot.
- **Large APIs.** `tabs.hide`, `sessions` values and `menus.overrideContext` touch tab and menu code in each browser. `contextualIdentities` depends on the containers work (Brave-based containers in dubna).
- **Machine load.** Corpus runs use the builds lock and follow the load rules.

## Status

| Area | State | Step |
|---|---|---|
| `browser.*` namespace with promises | In Chromium, also in MV2 | — |
| MV2 | Kept by ungoogled-chromium (oganesson) and Titanium (tennessine) | — |
| Corpus list | `corpus.json`: AMO's top 1000 by users on 2026-11-09, `.xpi` files cached by `tools/amo_corpus.py fetch` | #22 |
| Test harness | oganesson's `tools/e2e/harness.py`: an add-on from an `.xpi`, its ID, background, errors and popup | zakius/oganesson#63 |
| Corpus runner | oganesson's `tools/e2e/corpus.py`: each add-on's level, L0 to L2; `--only` checks the add-ons a step affects. A permission counts as known by its aliases too (Chromium's `Alias()` lists). A manifest key counts as missing only if Firefox reads it (MDN's manifest.json keys that Firefox supports, and `experiment_apis`): Firefox does nothing with the others. An error counts for the add-on whose page it comes from, not for one whose page its message names. A refusal that Firefox gives too doesn't count: scripting a browser page or another add-on's page (`FIREFOX_TOO`). The runner tells errors apart by their message, and Chromium gives the same ones for zoom and debugger calls on such pages, so for those the score errs high. L2: 823 → 831; the refusals, L2: 858 → 861 with the rows of #97 and #96 below (Dark Reader, Dark Background and Light Text, Milanote up) | zakius/oganesson#64, zakius/oganesson#84, zakius/oganesson#102 |
| Corpus score | `score/oganesson.json`, 2026-11-15, oganesson 157.0.8103.78: L0 1000, L1 997, L2 882. Some add-ons' levels vary between runs (zakius/oganesson#85) | zakius/oganesson#64 |
| Static scan | `tools/amo_corpus.py scan` writes `ranking.md`: on Chromium 157.0.8103.38, 110 missing manifest keys, permissions and APIs, most of them used by one add-on | #23 |
| `runtime.getBrowserInfo` | Both browsers (`firefox-add-ons.patch`). `name` is the browser's own, `version` Chromium's; an add-on listed in `firefox_runtime_api.cc` or in the profile pref `firefox_add_ons.named_firefox` (Gecko or extension IDs, edited by hand) gets `Firefox` and `Mozilla`. The list is empty so far. L2: 472 → 479 | #38 |
| `menus` namespace and permission | Both browsers (`firefox-add-ons.patch`): aliases of `contextMenus`, as Chromium aliases `networking.onc`. Firefox's own parts are #43; Firefox's `icons` and contexts in `menus.create` are #82. L2: 479 → 494, 9 of it from `menus`, the rest add-ons whose level varies between runs | #63 |
| Manifest V3 background pages | Both browsers (`firefox-add-ons.patch`): Firefox's `background.scripts` and `background.page` run in Manifest V3 as a persistent background page. A manifest that also lists a service worker runs the worker, as Chromium does. `runtime.getContexts()` lists the page as the background. Module backgrounds are #64; `page_action` in Manifest V3 is #36. L2: 494 → 696 | #35 |
| Version strings with a suffix | Both browsers (`firefox-add-ons.patch`, `firefox_version.cc`): a Firefox version such as `3.4resigned1`, `0.5rc2` or `1.3.4buildid20230720.091143` loads as its leading numbers, with the full string as the version name, unless the manifest has its own. L1: 867 → 922, L2: 696 → 739 | #62 |
| Module backgrounds | Both browsers (`firefox-add-ons.patch`): a background page from `background.scripts` with `"type": "module"`, in Manifest V2 or V3, loads its scripts as modules, as Firefox does. L2: 739 → 765 (30 with module backgrounds up; 1 other up and 5 down that vary between runs, oganesson#85) | #64 |
| Locale files | Both browsers (`firefox-add-ons.patch`): a message of any name ("first-install-body") loads, a placeholder that the message doesn't define becomes empty, and an unknown `__MSG_name__` in the manifest stays as it is, as in Firefox. L1: 922 → 932, L2: 765 → 772 (3 of the 10 locale add-ons reach L2; 4 other add-ons up that vary between runs, oganesson#85) | #34 |
| Shortcuts | Both browsers (`firefox-add-ons.patch`, `function_keys.cc`): Ctrl+Alt, F1 to F12 alone or with Shift, MacCtrl as Ctrl, more than four shortcuts, and commands without a description or without a key for this platform load, as in Firefox, for Chrome Web Store extensions too. The shortcuts page records F1 to F12 and Ctrl+Alt. `_execute_sidebar_action` is zakius/oganesson#86. L1: 932 → 964, L2: 772 → 784 (32 of the 41 add-ons up; 1 other up and 1 down that vary between runs, oganesson#85) | #33 |
| Event pages with webRequest | Both browsers (`firefox-add-ons.patch`): a Manifest V2 event page that asks for `webRequest` runs as a persistent background page, so its blocking listener is always there. Chromium refuses to load it. `storage.session` in Manifest V2 is #87. L1: 964 → 976, L2: 784 → 793 (12 of the 13 add-ons up, 9 to L2; 3 others up and 3 down that vary between runs, oganesson#85) | #32 |
| Page action next to the toolbar action | Both browsers (`firefox-add-ons.patch`, `firefox_page_action.cc`): a `page_action` next to `browser_action` or `action`, or in Manifest V3, loads as a second action, and `pageAction.*` reaches it. What a tab sets is reset when it navigates. The location bar button and `pageAction.openPopup` are zakius/oganesson#68. L1: 976 → 994, L2: 793 → 805 (all 18 add-ons up, 3 to L2; 12 others up and 3 down that vary between runs, oganesson#85) | #36 |
| Firefox's `menus.create` properties | Both browsers (`firefox-add-ons.patch`): `menus.create` and `update` take an item's `icons`, not shown, and the `password`, `bookmark` and `tools_menu` contexts, which nothing offers yet: the item shows in its other contexts. L2: 805 → 809 (9 of the 10 add-ons up; 3 others up, and 8 down that vary between runs or were L1 in every score but the last, oganesson#85) | #82 |
| Firefox's `browserSettings` | Both browsers (`firefox-add-ons.patch`, `firefox_browser_settings.cc`): read-only. `get()` gives the home page (on tennessine the new tab page, unless policy sets both the home page and HomepageIsNewTabPage off, zakius/tennessine#44), the new tab page, where new tabs open (oganesson's tab behavior), image animation, page colors and the notifications default from the browser's settings, and what Chromium always does for the rest. `set()` and `clear()` change nothing, and `onChange` doesn't fire. The permission warns "Read and change user and device settings". `verticalTabs` and `colorManagement` have no equal and are left out. `contextMenuShowEvent.set` is #94. L2: 809 → 823 (7 of the 14 add-ons up; 6 stop at another missing API, and Windscribe at a race, at L2 alone; 7 others up that vary between runs, oganesson#85) | #41 |
| `storage.session` in Manifest V2 | Both browsers (`firefox-add-ons.patch`): Manifest V2 add-ons get `storage.session`, as in Firefox. As in Manifest V3, it is in memory only, and content scripts reach it only after `setAccessLevel`. Ghostery stops at `scripting` in Manifest V2 (#31) next. L2: 831 → 835 (8 of the 9 add-ons up; 3 others up, and 7 down that vary between runs, oganesson#85) | #87 |
| Firefox's `theme`, read-only | Both browsers (`firefox-add-ons.patch`, `theme_api.cc`): `theme.getCurrent()` and `onUpdated` give the colours of the window's theme in Firefox's key names, to any add-on, as in Firefox. Images and properties are null; on tennessine the whole theme is Firefox's empty one. The `theme` permission loads, without a warning. `update()` and `reset()` are #95. Save Screenshot stops at `windowId` in `browserAction.setIcon` (#96) next, ColorfulTabs at a loading tab's URL (#97). L2: 835 → 846 (6 of the 12 add-ons up; Dark Reader reaches L2 alone; 7 others up and 2 down that vary between runs, oganesson#85) | #40 |
| `scripting` in Manifest V2 | Both browsers (`firefox-add-ons.patch`): Manifest V2 add-ons get the `scripting` permission and `browser.scripting`, as in Firefox. Firefox's own `contentScripts.register` is #70. L2: 846 → 847 (Privacy Badger and Ghostery up; 1 other down that varies between runs, oganesson#85) | #31 |
| Badge text colour and user settings in `browserAction` | Both browsers (`firefox-add-ons.patch`, `firefox_browser_action.h`): `browserAction.setBadgeTextColor`, `getBadgeTextColor` and `getUserSettings` run `action`'s functions, as in Firefox. A `null` colour, which Firefox takes to reset the colour, is refused, as in `action`; `windowId` is #96. L2: 847 → 850 (Ahrefs, Mino and Shoop up; 1 other up and 1 down that vary between runs, oganesson#85) | #65 |
| `privacy.network.peerConnectionEnabled` | Both browsers (`firefox-add-ons.patch`, `firefox_browser_settings.cc`): read-only, as `browserSettings`. `get()` needs the `privacy` permission and gives `true`; `set()` and `clear()` change nothing. Turning WebRTC off is a later step. L2: 850 → 851 (Happy Bonobo Disable WebRTC up; 1 other up and 1 down that vary between runs, oganesson#85) | #93 |
| `contentScripts.register` | Both browsers (`firefox-add-ons.patch`, `firefox_content_scripts.cc`): Manifest V2 add-ons register content scripts at run time, as in Firefox: files or code, JavaScript and CSS, in the isolated or the main world. `register()` resolves to an object whose `unregister()` removes them, and they also go when the page that registered them closes. The scripting API doesn't list or remove them. A file is relative to the page that registers it. `cookieStoreId` `firefox-default` keeps the scripts out of private windows; a container's ID is refused until add-ons have containers (#45). `cssOrigin: "user"` is taken but not followed yet. Content scripts' `wrappedJSObject`, `cloneInto` and `exportFunction` are #100. L2: 851 → 853 (BetterTTV, BTRoblox and Save webP as PNG up; User-Agent Switcher down: its page script, which now runs, reads `window.wrappedJSObject`, #100) | #70 |
| `homepage_url` from the locale files | Both browsers (`firefox-add-ons.patch`): `__MSG_name__` in `homepage_url` is replaced from the locale files, as in Firefox. Chromium read it as the URL and refused to load the add-on. L1: 994 → 997, L2: 853 → 858 for this step and the next: Tab Rotator up | #85 |
| `declarative_net_request` without its permission | Both browsers (`firefox-add-ons.patch`): a Manifest V3 add-on with `declarative_net_request` rules and neither `declarativeNetRequest` permission loads without its rules, with Chromium's error as an install warning, as in Firefox, which reads the rules only with the permission. DeepL and Trend Micro's toolbar up, L0 to L2; 4 others up and 2 down that vary between runs, oganesson#85 | #67 |
| A loading tab's URL | Both browsers (`firefox-add-ons.patch`, `firefox_add_on.h`): a tab that hasn't committed its first navigation has the URL `about:blank` for a Firefox add-on, as in Firefox; Chromium gives `""`, which `new URL()` refuses. A Firefox add-on is one whose manifest has Firefox's `gecko` key: other extensions, the bundled uBlock Origin among them, keep `""`, and so do AMO add-ons without the key (37 of the corpus) until the `.xpi` install step can tell where an add-on comes from. ColorfulTabs gets past it and stops at `theme.update` (#95), so no level changes. uBlock Origin from AMO held L2 in 93 group runs in a row | #97 |
| `windowId` in the action setters and getters | Both browsers (`firefox-add-ons.patch`, `firefox_action_window.h`): `browserAction`'s and `action`'s setters and getters, `setIcon` among them, take `windowId`, a value for the tabs of one window, under a tab's own value and over the global one, as in Firefox; `windows.WINDOW_ID_CURRENT` is the current window. A window's badge text wins over the declarativeNetRequest action count, as a tab's own does. A window's value is kept under the window's ID, in the same maps as the tabs' values. With a `tabId` too, or for no window, the call fails with Firefox's message. `pageAction` takes only `tabId`, as in Firefox. Save Screenshot and Cookie Auto Decline up; Save Screenshot stops at its content script's `let top` (#102) when a page runs it. L2: 858 → 861 with the row above and the runner's refusals (1 other up and 3 down that vary between runs, oganesson#85) | #96 |
| Firefox's `contextualIdentities`, read-only | Both browsers (`firefox-add-ons.patch`, `contextual_identities_api.cc`, `firefox_cookie_store.h`): `contextualIdentities.query` and `get` give the browser's containers in Firefox's shape, with the `cookieStoreId` `firefox-container-<id>`, as the containers sync writes it in Multi-Account Containers' format. `iconUrl` is Firefox's own, which only Firefox loads. `onCreated`, `onUpdated` and `onRemoved` follow the user's list. The permission loads, without a warning. Tabs carry `cookieStoreId`: `firefox-default`, `firefox-private` or a container's, and the cookies API takes `firefox-default` and `firefox-private` as Chromium's two stores. `create`, `update`, `remove` and `move` are #103, `cookieStoreId` in `tabs.create` and `tabs.query` #104, `firstPartyDomain` #105, a container's cookies in the cookies API #106. L2: 861 → 882 with the row below (16 of the 27 add-ons at the permission up; Cookie AutoDelete stops at `firstPartyDomain`, Temporary Containers and RoQoL reach L2 in some runs; 2 others up and 1 down that vary between runs, oganesson#85) | #45 |
| Filters on `tabs.onUpdated` | Both browsers (`firefox-add-ons.patch`, `firefox_update_filter.cc`): `tabs.onUpdated.addListener()` takes Firefox's filter, `urls`, `properties`, `tabId`, `windowId` and `cookieStoreId`, and the listener runs only for the updates that match. Firefox's properties that Chromium's changes lack (`hidden`, `isArticle`, `attention`, `sharingState`) never match. Adaptive Tab Bar Colour, The Camelizer, Remove YouTube Shorts and Default Bookmark Folder up, and Pouch starts without an error. L2 in the row above | #30 |

Add a row for each step that ships.
