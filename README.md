# dubna

dubna holds the code shared by two Chromium-based browsers:

| Browser | Platform | Built on |
|---|---|---|
| [oganesson](https://github.com/zakius/oganesson) | Desktop: Linux, Windows planned | [Helium](https://github.com/imputnet/helium), through [helium-linux](https://github.com/imputnet/helium-linux) |
| [tennessine](https://github.com/zakius/tennessine) | Android | [Vanadium](https://github.com/GrapheneOS/Vanadium), with [Titanium](https://github.com/jqssun/android-titanium-browser)'s extension support |

It contains the Chromium milestone both browsers build, and the code, patches and tools they both use.

> **Personal project.** Like both browsers, dubna changes without notice, and development may stop at any moment.

## What it's for

The two browsers share a large part of their code, but they are different products. tennessine on a large Android screen doesn't give you the oganesson experience. The platforms differ in toolbars, tab strips, windows, extensions and input, and each browser is built around its own platform. That is why the browsers have separate names, repos and releases.

dubna is where the parts they share are synthesized.

## The name

Both browsers are named after elements. oganesson takes its name from element 118 because that element is in helium's group, and the browser is built on Helium. tennessine takes its name from element 117, the neighbour of 118. Both elements were first synthesized at the Joint Institute for Nuclear Research in Dubna, oganesson in 2002 with Lawrence Livermore National Laboratory, and tennessine in 2010 with Oak Ridge National Laboratory and other American labs. That coincidence gave this repo its name. The town has an element of its own too, dubnium (105).

## Layout

| Path | What |
|---|---|
| `milestone.txt` | The Chromium milestone both browsers build. |
| `src/` | New files both browsers copy over the Chromium tree before their own: the C++ of Firefox-compatible Sync (`chrome/browser/firefox_sync/`), its strings, and the headers for no saved history. Nothing here is platform-specific. What Sync needs from the browser (tabs, the sign-in tab, synced-tab views, the account on the profile, the library's path) is declared in `firefox_sync_platform.h`, which each browser implements in its own `chrome/browser/firefox_sync/platform/`. |
| `patches/` | Patches both browsers apply: `firefox-sync.patch` (the service, on Linux and Android), `no-saved-history.patch` (which registers its own prefs) and `ublock-assets-update.patch` (uBlock Origin's filter lists from uBlock Origin's own URLs, instead of Helium's servers). |
| `rust/oganesson_sync/` | The Rust library of Firefox-compatible Sync. Each browser builds it with its own `build-sync-lib.sh`: oganesson (`linux/`) with Chromium's toolchain, tennessine (`tools/`) with the NDK. |
| `l10n/` | Polish translations of the strings in `src/`. |
| `tools/` | `upstream_status.py` (the update policy check), `translate.py` (puts translations into a tree, for both browsers), `update_app_services.py` (moves the Rust library to a new application-services release). |

Both browsers include dubna as a submodule. oganesson copies `dubna/src/` in before its own `src/`, and its `patches/series` lists `../dubna/patches/…`. In tennessine the submodule sits next to Vanadium, Titanium and Helium. tennessine takes the uBlock Origin patches and pin from Helium, at the commit that oganesson's `helium` submodule pins.

A shared feature registers its own prefs and strings, so it doesn't depend on hooks in either browser. Each browser's patch check warns when its Chromium version is off `milestone.txt`.

Before both browsers move their submodule to a change here, check the change with both browsers' patch checks and with a build of each.

## Tools

Each browser has one entry point: `oganesson.py` for oganesson, `tennessine.py` for tennessine. Shared jobs are Python tools in this repo, stdlib only.

Scripts that run inside a build container or come from an upstream stay in bash. Those are oganesson's `linux/container-build.sh` and the helium-linux scripts it runs, and Titanium's `build.sh`, `common.sh` and `post-sync.sh`. The two `build-sync-lib.sh` scripts also stay in bash, and they share nothing but the crate.

## Update policy

Each browser takes its upstream's patches as they are, so the upstream does the rebasing. A patch shared by both browsers has exactly one version, so both browsers must build the same Chromium milestone.

1. **New Chromium release in the same milestone: take it now.** These releases carry security fixes, and shared patches rarely break between them. Each browser moves as soon as its own upstream has the release.
2. **New milestone: wait until every upstream has it.** Then move both browsers and `milestone.txt` together. While one upstream lags, the other browser stays on the current milestone, at its upstream's newest release in that milestone.
3. **Exploited in the wild: update now.** When Chromium fixes an exploit that is used in the wild, each browser takes the first upstream release with the fix at once. It does so even into a new milestone, before the other upstream gets there. If its upstream has no such release yet, bump the browser's Chromium version ourselves and fix the upstream patches that fail. Those fixes stay ours only until the upstream catches up. In September 2026 both upstreams shipped each zero-day fix within about a day, so the upstream usually comes first.
4. **Stable ahead for long.** When Chromium stable has been newer than the policy allows for more than 7 days, read the release notes and decide whether to act as in rule 3. Nearly every release has fixes rated High, so severity alone doesn't decide it.

`tools/upstream_status.py` applies these rules and prints what is due:

```sh
tools/upstream_status.py --desktop ../oganesson --android ../tennessine
```

| Finding | Meaning |
|---|---|
| `update` | Rule 1: the browser's upstream has a newer release in the milestone. |
| `milestone` | Rule 2: every upstream has a newer milestone. |
| `waiting` | Rule 2: one upstream has a newer milestone and the other doesn't. Nothing to do. |
| `zeroday` | Rule 3: a version we build has an exploit in the wild. |
| `security` | Rule 4: Chromium stable has been ahead for more than 7 days. |

Google marks zero-days only with the sentence "Google is aware that an exploit for CVE-… exists in the wild" in its Chrome Releases post. The script collects those CVEs and adds the Chromium entries from CISA's [Known Exploited Vulnerabilities](https://www.cisa.gov/known-exploited-vulnerabilities-catalog) catalog. It then finds the release that fixes each one in the posts. A CVE that the script can't place is the one case that needs a person to read the posts.

The script needs only Python 3 and the network. `tools/test_upstream_status.py` checks the rules without the network.

A T3 scheduled task runs the report every night at midnight and does the preparation that a finding calls for. It doesn't commit, push or start a long build without the maintainer.

## License

GPL-2.0 or GPL-3.0, at your option, so that both browsers can build dubna in: tennessine is GPL-2.0, because Vanadium's patches are GPL-2.0-only, and oganesson is GPL-3.0, because Helium's are. Under section 14 of GPL-3.0 the maintainer is the proxy who decides whether a future GPL version can be used too. See `LICENSE`.
