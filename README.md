# dubna

dubna holds the code shared by two Chromium-based browsers:

| Browser | Platform | Built on |
|---|---|---|
| [oganesson](https://github.com/zakius/oganesson) | Desktop: Linux and Windows | [Helium](https://github.com/imputnet/helium), through [helium-linux](https://github.com/imputnet/helium-linux) and [helium-windows](https://github.com/imputnet/helium-windows) |
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
| `src/` | New files both browsers copy over the Chromium tree before their own: the C++ of Firefox-compatible Sync (`chrome/browser/firefox_sync/`), its strings, the headers for no saved history, and browser traffic's rules (`components/browser_traffic/`): what the browser downloads by itself, whether it goes direct or through the user's helium-services server, and the presets, with their unit tests (`browser_traffic_unittests`). Each browser wires its own downloads to them; only oganesson does so far. Nothing here is platform-specific. What Sync needs from the browser (tabs, the sign-in tab, tabs other devices send or close, the account on the profile, the library's path) is declared in `firefox_sync_platform.h`, which each browser implements in its own `chrome/browser/firefox_sync/platform/`. Links out of installed web apps (`chrome/browser/app_links/`, with the page-side check in `third_party/blink/renderer/core/html/app_links.cc`): a link that leaves an app's scope opens in the default browser, and the user's per-app scope rules. What it needs from the browser (which app a page is in, its scope, the default browser) is declared in `app_links_platform.h`, implemented in each browser's `chrome/browser/app_links/platform/`. |
| `patches/` | Patches both browsers apply: `firefox-sync.patch` (the service, on Linux, Windows and Android), `firefox-sync-remote-tabs.patch` (Chromium's synced sessions show Firefox Sync's tabs from other devices), `no-saved-history.patch` (which registers its own prefs), and `app-links.patch` (links out of installed web apps: the page asks before its address changes, the throttle, the host, and the scope rules' prefs). |
| `rust/oganesson_sync/` | The Rust library of Firefox-compatible Sync. Each browser builds it with its own `build-sync-lib.sh`: oganesson (`linux/`, `windows/`) with Chromium's toolchain, tennessine (`tools/`) with the NDK. |
| `l10n/` | Polish translations of the strings in `src/`. |
| `tools/` | `upstream_status.py` (the update policy check), `translate.py` (puts translations into a tree, for both browsers), `update_app_services.py` (moves the Rust library to a new application-services release), `ours.py` (lists the objects our C++ files compile to, so a product's build can compile them first), `release_notes.py` (a GitHub release's notes, from the commits since the previous release). |
| `webextensions/` | The plan, the rules and the status of the Firefox WebExtensions climb, with its corpus list and scores. |
| `site/` | dubna's site (https://dubna.zakius.xyz): `site.py` builds, publishes and checks it; `server/` holds the server's compose and nginx files. |

Both browsers include dubna as a submodule. oganesson copies `dubna/src/` in before its own `src/`, and its `patches/series` lists `../dubna/patches/…`. In tennessine the submodule sits next to Vanadium and Titanium.

A shared feature registers its own prefs and strings, so it doesn't depend on hooks in either browser. Each browser's patch check warns when its Chromium version is off `milestone.txt`.

Before both browsers move their submodule to a change here, check the change with both browsers' patch checks and with a build of each.

## Tools

Each browser has one entry point: `oganesson.py` for oganesson, `tennessine.py` for tennessine. Shared jobs are Python tools in this repo, stdlib only.

Scripts that run inside a build container or come from an upstream stay in bash. Those are the `container-build.sh` scripts (oganesson's `linux/` and `windows/`, tennessine's `tools/`), the helium-linux scripts oganesson's Linux one runs, Titanium's `build.sh` and `common.sh`, and tennessine's `post-sync.sh`, which `build.sh` runs. The `build-sync-lib.sh` scripts also stay in bash, and they share nothing but the crate.

## Update policy

Each browser takes its upstream's patches as they are, so the upstream does the rebasing. A patch shared by both browsers has exactly one version, so both browsers must build the same Chromium milestone.

1. **New Chromium release in the same milestone: take it now.** These releases carry security fixes. Each browser moves as soon as its own upstream has the release, so the two may build different releases of one milestone for a while. That is an experiment: Chromium's version numbers say nothing about internal changes, but shared patches have so far fit every release of a milestone. Each browser's patch check applies the shared patches to its own release, so a break shows there. If breaks become common, both browsers wait for the release both upstreams have instead.
2. **New milestone: wait until every upstream has it.** Then move both browsers and `milestone.txt` together. While one upstream lags, the other browser stays on the current milestone, at its upstream's newest release in that milestone.
3. **Exploited in the wild: update now.** When Chromium fixes an exploit that is used in the wild, each browser takes the first upstream release with the fix at once. It does so even into a new milestone, before the other upstream gets there. If its upstream has no such release yet, bump the browser's Chromium version ourselves and fix the upstream patches that fail (Helium's, or Vanadium's and Titanium's). Those fixes stay ours only until the upstream catches up. In September 2026 both upstreams shipped each zero-day fix within about a day, so the upstream usually comes first.
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
| `zeroday` | Rule 3: a version I build has an exploit in the wild. |
| `security` | Rule 4: Chromium stable has been ahead for more than 7 days. |

Google marks zero-days only with the sentence "Google is aware that an exploit for CVE-… exists in the wild" in its Chrome Releases post. The script collects those CVEs and adds the Chromium entries from CISA's [Known Exploited Vulnerabilities](https://www.cisa.gov/known-exploited-vulnerabilities-catalog) catalog. It then finds the release that fixes each one in the posts. A CVE that the script can't place is the one case that needs a person to read the posts.

The script needs only Python 3 and the network. `tools/test_upstream_status.py` checks the rules without the network.

A T3 scheduled task runs the report every evening at 18:00 and does the whole update that a finding calls for: it fixes what breaks, builds, tests and pushes, then reports every fix for me to review. It stops before the push when a build or test failure has no clear, small fix. When no update is due, it also takes a batch of clear open issues to the end, in one build and one push per browser. The batch is worth at most 6 points: a `size: nit` issue counts 1, a `size: minor` 2 and a `size: medium` 6, so it takes up to 5 nits, 3 minors or 1 medium. It never takes an issue labelled `blocked`, whose body or a comment says what it waits for. When no update was due and the batch left time, it does one step of the Firefox WebExtensions climb (`webextensions/README.md`). Last, it writes what the site's patch list and pages are missing (`site/site.py check`), including what the run landed, and pushes it to main, so the site ships the same night.

Another task reads other browsers' changelogs every morning at 09:00 and publishes what is worth a look as a feed (https://browser-watch.zakius.xyz/feed.xml). It files an issue when the goal is already settled, and labels it `blocked` when the issue is likely wanted but needs my OK or a later Chromium.

## Releases

A push to a browser's main branch releases it. GitHub Actions run each repo's workflow (`.github/workflows/`) on this project's own build machine, with a self-hosted runner per repo (labels `oganesson`, `tennessine` and `dubna`), because a Chromium build doesn't fit GitHub's machines. The workflow builds, tests and publishes from the main checkout, then posts a GitHub release with the packages and notes from the commits since the previous release (`tools/release_notes.py`). Pushes that change only docs or CI don't start one. Each workflow can also be started by hand.

A merge to dubna's main that changes `site/` publishes the site. Pull requests start nothing: the runners would run their code on the build machine.

## License

GPL-2.0 or GPL-3.0, at your option, so that both browsers can build dubna in: tennessine is GPL-2.0, because Vanadium's patches are GPL-2.0-only, and oganesson is GPL-3.0, because Helium's are. Under section 14 of GPL-3.0 I am the proxy who decides whether a future GPL version can be used too. See `LICENSE`.
