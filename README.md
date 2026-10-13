# dubna

The layer shared by two Chromium-based browsers:

- [oganesson](https://github.com/zakius/oganesson-focused): desktop (Linux, Windows planned), on [Helium](https://github.com/imputnet/helium) through [helium-linux](https://github.com/imputnet/helium-linux).
- [tennessine](https://github.com/zakius/tennessine-focused): Android, on [Vanadium](https://github.com/GrapheneOS/Vanadium) with [Titanium](https://github.com/jqssun/android-titanium-browser)'s extension support.

The products keep their own names, repos and releases. This repo holds what both build on: the shared Chromium milestone, and the code, patches and tools that both use.

## Update policy

Each product takes its upstream's patches as they are, so that the upstream does the rebasing. A patch shared by both products has exactly one version, so both products must build the same Chromium milestone.

1. **New Chromium release in the same milestone: take it now.** These releases carry security fixes, and shared patches rarely break between them. Each product moves as soon as its own upstream has the release.
2. **New milestone: wait until every upstream has it.** Then move both products and `milestone.txt` together. While one upstream lags, the other product stays on the current milestone, at that upstream's newest release in it.
3. **Exploited in the wild: update now.** When Chromium fixes an exploit that is used in the wild, each product takes the first upstream release with the fix at once, even into a new milestone before the other upstream gets there. When its upstream has no such release yet, bump the product's Chromium version ourselves and fix the upstream patches that fail. Those fixes stay ours only until the upstream catches up. In September 2026 both upstreams shipped each zero-day fix within about a day, so the upstream usually comes first.
4. **Stable ahead for long.** When Chromium stable has been newer than what the policy allows for more than 7 days, read the release notes and decide whether to do the same as in rule 3. Nearly every release has fixes rated High, so severity alone doesn't decide it.

Google marks zero-days only with the sentence "Google is aware that an exploit for CVE-… exists in the wild" in the Chrome Releases post. The script takes those CVEs, adds the Chromium entries in CISA's [Known Exploited Vulnerabilities](https://www.cisa.gov/known-exploited-vulnerabilities-catalog) catalog, and finds the release that fixes each one in the posts. A CVE that it can't place is the one case that needs a reader.

`tools/upstream_status.py` applies these rules and prints what is due:

```sh
tools/upstream_status.py --desktop ../oganesson-focused --android ../tennessine-focused
```

| Finding | Meaning |
|---|---|
| `update` | Rule 1: the product's upstream has a newer release in the milestone. |
| `milestone` | Rule 2: every upstream has a newer milestone. |
| `waiting` | Rule 2: one upstream has a newer milestone and the other doesn't. Nothing to do. |
| `zeroday` | Rule 3: a version we build has an exploit in the wild. |
| `security` | Rule 4: Chromium stable has been ahead for more than 7 days. |

It needs only Python 3 and the network. `tools/test_upstream_status.py` checks the rules without the network. A T3 scheduled task runs the report every morning and does the preparation that a finding calls for. It doesn't commit, push or start a long build without the maintainer.

## Moving shared code here

Today tennessine-focused has oganesson-focused as a submodule and copies the shared files out of it. The submodule is pinned, so the two drift apart, and a change on desktop can break Android without anything showing it. Desktop's `ba35311` did: it moved the no-saved-history pref registration into the desktop-only `hooks.patch`.

Steps, each one verified with both products' patch checks:

1. Give the shared features (Firefox Sync, no saved history) their own pref and string registration, apart from desktop's `oganesson_prefs.h`.
2. Move here: `src/chrome/browser/firefox_sync/`, its strings and their Polish translations, `rust/oganesson_sync/`, `no-saved-history.patch` and its headers, and the `[ublock_origin]` pin from `deps.ini`.
3. Both products add dubna as a submodule. tennessine drops its oganesson submodule. Android takes Helium's uBlock Origin patches from desktop's vendored Helium copy, now in dubna, at the shared milestone.
4. Each product's patch check fails when its upstream's Chromium milestone differs from `milestone.txt`.

## One tool per job

The tools that both products need move here as one Python script each (stdlib, Python 3.10). Files that come from an upstream (helium-linux's `linux/`, Titanium's `build.sh` and `common.sh`) stay as they are.

| Job | Desktop today | Android today |
|---|---|---|
| Build the Sync Rust library | `tools/build-sync-lib.sh` | `tools/build-sync-lib.sh`, a separate copy |
| Apply the series, zero fuzz | `tools/check_patches.py` | `check.sh`, `apply.sh` |
| Refresh a failing patch | `tools/regen.py` | `tools/refresh.sh` |
| Report upstream changes | `tools/update_helium.py --dry-run` | `tools/upstream.sh` |
| Apply our translations | `tools/translate.py` | uses desktop's |

Each product keeps one entry point: `oganesson.py` on desktop. On Android, `apply.sh` and `local-build.sh` become one Python entry point.
