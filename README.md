# dubna

The layer shared by two Chromium-based browsers:

- [oganesson](https://github.com/zakius/oganesson): desktop (Linux, Windows planned), on [Helium](https://github.com/imputnet/helium) through [helium-linux](https://github.com/imputnet/helium-linux).
- [tennessine](https://github.com/zakius/tennessine): Android, on [Vanadium](https://github.com/GrapheneOS/Vanadium) with [Titanium](https://github.com/jqssun/android-titanium-browser)'s extension support.

The two browsers share a large part of their code, but they are different products. tennessine on a large Android screen doesn't give you oganesson: the platforms differ in their toolbars, tab strips, windows, extensions and input, and each browser is built around its own platform. So they have their own names, repos and releases. This repo holds what both build on: the shared Chromium milestone, and the code, patches and tools that both use.

Both elements were first synthesized at the Joint Institute for Nuclear Research in Dubna: oganesson (118) in 2002, tennessine (117) in 2010. This is where the parts they share are made. The town also has an element of its own, dubnium (105).

Like both browsers, this is a personal project: it changes without notice, and development may stop at any moment.

## Update policy

Each product takes its upstream's patches as they are, so that the upstream does the rebasing. A patch shared by both products has exactly one version, so both products must build the same Chromium milestone.

1. **New Chromium release in the same milestone: take it now.** These releases carry security fixes, and shared patches rarely break between them. Each product moves as soon as its own upstream has the release.
2. **New milestone: wait until every upstream has it.** Then move both products and `milestone.txt` together. While one upstream lags, the other product stays on the current milestone, at that upstream's newest release in it.
3. **Exploited in the wild: update now.** When Chromium fixes an exploit that is used in the wild, each product takes the first upstream release with the fix at once, even into a new milestone before the other upstream gets there. When its upstream has no such release yet, bump the product's Chromium version ourselves and fix the upstream patches that fail. Those fixes stay ours only until the upstream catches up. In September 2026 both upstreams shipped each zero-day fix within about a day, so the upstream usually comes first.
4. **Stable ahead for long.** When Chromium stable has been newer than what the policy allows for more than 7 days, read the release notes and decide whether to do the same as in rule 3. Nearly every release has fixes rated High, so severity alone doesn't decide it.

Google marks zero-days only with the sentence "Google is aware that an exploit for CVE-… exists in the wild" in the Chrome Releases post. The script takes those CVEs, adds the Chromium entries in CISA's [Known Exploited Vulnerabilities](https://www.cisa.gov/known-exploited-vulnerabilities-catalog) catalog, and finds the release that fixes each one in the posts. A CVE that it can't place is the one case that needs a reader.

`tools/upstream_status.py` applies these rules and prints what is due:

```sh
tools/upstream_status.py --desktop ../oganesson --android ../tennessine
```

| Finding | Meaning |
|---|---|
| `update` | Rule 1: the product's upstream has a newer release in the milestone. |
| `milestone` | Rule 2: every upstream has a newer milestone. |
| `waiting` | Rule 2: one upstream has a newer milestone and the other doesn't. Nothing to do. |
| `zeroday` | Rule 3: a version we build has an exploit in the wild. |
| `security` | Rule 4: Chromium stable has been ahead for more than 7 days. |

It needs only Python 3 and the network. `tools/test_upstream_status.py` checks the rules without the network. A T3 scheduled task runs the report every night at midnight and does the preparation that a finding calls for. It doesn't commit, push or start a long build without the maintainer.

## Layout

| Path | What |
|---|---|
| `milestone.txt` | The Chromium milestone both products build. |
| `src/` | New files both products copy over the Chromium tree before their own: Firefox Sync's C++ (`chrome/browser/firefox_sync/`), its strings, and no saved history's headers. Nothing platform-specific: what Sync needs from the browser (tabs, the sign-in tab, synced-tab views, the account on the profile, the library's path) is `firefox_sync_platform.h`, which each product implements in its `chrome/browser/firefox_sync/platform/`. |
| `patches/` | Patches both products apply: `firefox-sync.patch` (the service, on Linux and Android), `no-saved-history.patch` (which registers its own prefs) and `ublock-assets-update.patch` (uBlock Origin's filter lists from uBlock's own URLs, instead of Helium's servers). |
| `rust/oganesson_sync/` | Firefox Sync's Rust library. Each product builds it with its own `build-sync-lib.sh`: desktop (`linux/`) with Chromium's toolchain, Android (`tools/`) with the NDK. |
| `l10n/` | Polish translations of the strings in `src/`. |
| `tools/` | `upstream_status.py` (the policy check), `translate.py` (puts translations into a tree, for both products), `update_app_services.py` (moves the Rust library to a new application-services release). |

Desktop has dubna as a submodule: `dubna/src/` goes in before `src/`, and `patches/series` lists `../dubna/patches/…`. tennessine has it as a submodule too, next to Vanadium, Titanium and Helium (uBlock Origin's patches and pin, at the commit desktop's `helium` submodule pins). A shared feature registers its own prefs and strings, so it doesn't depend on either product's hooks. Each product's patch check warns when its Chromium is off `milestone.txt`.

A change here is checked with both products' patch checks, and with a build of each, before both move their submodule to it.

## Tools

Each product has one entry point: `oganesson.py` on desktop, `tennessine.py` on Android. Shared jobs are Python tools here, stdlib only. Scripts that run inside a build container or come from an upstream (desktop's `linux/container-build.sh` and the helium-linux scripts it runs, Titanium's `build.sh`, `common.sh` and `post-sync.sh`) stay in bash, and so do the two `build-sync-lib.sh`, which share nothing but the crate.
