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

The evening task (dubna `README.md`, "Update policy") does one climb step when:

- no update was due, and
- the tracker batch was empty, or it was small enough that a step can still be released by 06:00. The agent decides.

The 06:00 is the first one after the run starts: a run started early in the day, by hand or on schedule, works on until then. Builds run at any hour; during the day the system gives them fewer CPUs.

It takes the open `climb` step with the best gain for its size whose `Depends on` steps are closed. It doesn't take a `blocked` step.

If a step isn't released by about 06:00, the agent stops it, pushes the work to a branch `climb-<repo>-<issue>`, and writes in the issue what is done and what is left. The next run continues it. If the second run can't finish it either, the agent releases the part that works (if it is useful alone), files the rest as new steps, and closes the issue.

When the last full corpus run is more than 7 days old, the climb work for that evening is a full corpus run and a score update instead of a step.

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
| Corpus runner | oganesson's `tools/e2e/corpus.py`: each add-on's level, L0 to L2; `--only` checks the add-ons a step affects. A permission counts as known by its aliases too (Chromium's `Alias()` lists) | zakius/oganesson#64 |
| Corpus score | `score/oganesson.json`, 2026-11-11, oganesson 157.0.8103.50: L0 1000, L1 922, L2 739. Some add-ons' levels vary between runs (zakius/oganesson#85) | zakius/oganesson#64 |
| Static scan | `tools/amo_corpus.py scan` writes `ranking.md`: on Chromium 157.0.8103.38, 110 missing manifest keys, permissions and APIs, most of them used by one add-on | #23 |
| `runtime.getBrowserInfo` | Both browsers (`firefox-add-ons.patch`). `name` is the browser's own, `version` Chromium's; an add-on listed in `firefox_runtime_api.cc` or in the profile pref `firefox_add_ons.named_firefox` (Gecko or extension IDs, edited by hand) gets `Firefox` and `Mozilla`. The list is empty so far. L2: 472 → 479 | #38 |
| `menus` namespace and permission | Both browsers (`firefox-add-ons.patch`): aliases of `contextMenus`, as Chromium aliases `networking.onc`. Firefox's own parts are #43; Firefox's `icons` and contexts in `menus.create` are #82. L2: 479 → 494, 9 of it from `menus`, the rest add-ons whose level varies between runs | #63 |
| Manifest V3 background pages | Both browsers (`firefox-add-ons.patch`): Firefox's `background.scripts` and `background.page` run in Manifest V3 as a persistent background page. A manifest that also lists a service worker runs the worker, as Chromium does. Module backgrounds are #64; `page_action` in Manifest V3 is #36. L2: 494 → 696 | #35 |
| Version strings with a suffix | Both browsers (`firefox-add-ons.patch`, `firefox_version.cc`): a Firefox version such as `3.4resigned1`, `0.5rc2` or `1.3.4buildid20230720.091143` loads as its leading numbers, with the full string as the version name, unless the manifest has its own. L1: 867 → 922, L2: 696 → 739 | #62 |

Add a row for each step that ships.
