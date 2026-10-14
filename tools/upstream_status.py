#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Reports what is due for the upstreams under the policy in README.md.

Usage: tools/upstream_status.py [--desktop OGANESSON_CHECKOUT] [--android TENNESSINE_CHECKOUT]

Reads the shared milestone from milestone.txt, each product's current Chromium
version from its checkout (when given), the releases of helium-linux, Vanadium
and Titanium from GitHub, and Chromium's Linux stable releases from
chromiumdash, and the exploits Google reports in the wild from the Chrome
Releases blog. The first line is "status: current" or "status: action"; each
later line is one finding. GITHUB_TOKEN, when set, goes to api.github.com only.
"""
import argparse, base64, datetime, html, json, os, re, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GH = 'https://api.github.com'
# ponytail: one grace period for every Chromium release; the agent reads the
# release notes for severity when it fires. Split by severity if 7 days proves wrong.
GRACE = datetime.timedelta(days=7)
# Product: (upstream it follows, its GitHub repo).
PRODUCTS = {'oganesson': ('helium-linux', 'imputnet/helium-linux'),
            'tennessine': ('vanadium', 'GrapheneOS/Vanadium')}


def version(text):
    m = re.search(r'(\d+)\.(\d+)\.(\d+)\.(\d+)', text)
    return tuple(map(int, m.groups())) if m else None


def show(v):
    return '.'.join(map(str, v))


def get(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'dubna-upstream-status'})
    if url.startswith(GH + '/'):
        req.add_header('Accept', 'application/vnd.github+json')
        if os.environ.get('GITHUB_TOKEN'):
            req.add_header('Authorization', f'Bearer {os.environ["GITHUB_TOKEN"]}')
    with urllib.request.urlopen(req, timeout=30) as response:
        return json.load(response)


def github_releases(repo):
    """(tag, publish date) of each published release, newest first."""
    return [(r['tag_name'], datetime.date.fromisoformat(r['published_at'][:10]))
            for r in get(f'{GH}/repos/{repo}/releases?per_page=30')
            if not r['draft'] and not r['prerelease']]


def helium_linux_chromium(tag):
    """helium-linux tags don't name Chromium: its helium-chromium submodule's
    chromium_version.txt does."""
    sha = get(f'{GH}/repos/imputnet/helium-linux/contents/helium-chromium?ref={tag}')['sha']
    file = get(f'{GH}/repos/imputnet/helium/contents/chromium_version.txt?ref={sha}')
    return version(base64.b64decode(file['content']).decode())


def upstream_releases(name, repo, milestone):
    """(Chromium version, date, tag) of the releases at or above milestone,
    newest first, and at least the newest one."""
    found = []
    for tag, date in github_releases(repo):
        v = helium_linux_chromium(tag) if name == 'helium-linux' else version(tag)
        if v is None:
            continue
        if found and v[0] < milestone:
            break
        found.append((v, date, tag))
    return found


def chromium_stable():
    """(version, date) of Chromium's recent Linux stable releases. Vanadium
    tags follow the desktop version numbers, so Linux stands for both."""
    url = 'https://chromiumdash.appspot.com/fetch_releases?channel=Stable&platform=Linux&num=30'
    return [(version(r['version']), datetime.date.fromtimestamp(r['time'] / 1000))
            for r in get(url)]


KEV = 'https://www.cisa.gov/sites/default/files/feeds/known_exploited_vulnerabilities.json'


def zero_days(today):
    """([(fixed version, date, CVEs)], [unplaced CVE]) for the Chromium exploits
    known in the wild. The CVEs come from two sources: the Chrome Releases
    sentences that say "in the wild" (Google's only marker; severity is no
    sign), and the Chromium entries CISA added to its Known Exploited
    Vulnerabilities catalog in the last 30 days. Each CVE is fixed in the
    earliest recent desktop post that names it; the first full version in a
    post is the Linux one. An unplaced CVE needs a reader."""
    feed = get('https://chromereleases.googleblog.com/feeds/posts/default'
               '?alt=json&max-results=50')['feed']['entry']
    posts = []
    for post in feed:
        text = re.sub(r'\s+', ' ', html.unescape(re.sub('<[^>]+>', ' ', post['content']['$t'])))
        if 'Desktop' in post['title']['$t'] and version(text):
            posts.append((datetime.date.fromisoformat(post['published']['$t'][:10]),
                          version(text), text))
    wild = {cve for _, _, text in posts for sentence in re.findall(r'[^.]*in the wild[^.]*', text)
            for cve in re.findall(r'CVE-\d+-\d+', sentence)}
    recent = (today - datetime.timedelta(days=30)).isoformat()
    wild |= {v['cveID'] for v in get(KEV)['vulnerabilities']
             if v['vendorProject'] == 'Google' and 'Chromium' in v['product']
             and v['dateAdded'] >= recent}
    fixes, unplaced = {}, []
    for cve in sorted(wild):
        naming = sorted((date, v) for date, v, text in posts if cve in text)
        if naming:
            fixes.setdefault(naming[0][::-1], []).append(cve)
        else:
            unplaced.append(cve)
    return [(v, date, cves) for (v, date), cves in fixes.items()], unplaced


def current_versions(desktop, android):
    current = {}
    if desktop:
        current['oganesson'] = version((Path(desktop) / 'chromium_version.txt').read_text())
    if android:
        current['tennessine'] = version((Path(android) / 'vanadium/args.gn').read_text())
    return current


def assess(milestone, releases, stable, current, today, zerodays=()):
    """Findings, each (kind, text). releases: product -> [(version, date, tag)];
    stable: [(version, date)]; current: product -> version, for the given ones;
    zerodays: [(fixed version, date, CVEs)]. Kinds: zeroday, milestone, waiting,
    update, security."""
    findings = []
    newest_ms = {p: max(v[0] for v, _, _ in rs) for p, rs in releases.items()}
    ahead = {p: min(d for v, d, _ in rs if v[0] > milestone)
             for p, rs in releases.items() if newest_ms[p] > milestone}
    target = milestone
    if len(ahead) == len(releases):
        target = min(newest_ms.values())
        findings.append(('milestone', f'every upstream has Chromium {target}: move both '
                                      f'products and milestone.txt to {target}'))
    for lagging in (releases.keys() - ahead.keys()) if ahead else ():
        leader = min(ahead, key=ahead.get)
        findings.append(('waiting', f'{lagging}: {PRODUCTS[lagging][0]} is still on '
                                    f'{milestone}; {PRODUCTS[leader][0]} moved on '
                                    f'{ahead[leader]} ({(today - ahead[leader]).days} days)'))
    for p, rs in releases.items():
        allowed = ([r for r in rs if r[0][0] == target]
                   or [r for r in rs if r[0][0] <= target])
        if not allowed:
            continue
        best, _, tag = max(allowed)
        if p in current and best > current[p]:
            findings.append(('update', f'{p}: {PRODUCTS[p][0]} {tag} has Chromium '
                                       f'{show(best)}, we build {show(current[p])}'))
        since = [d for v, d in stable if v > best]
        if since and today - min(since) > GRACE:
            findings.append(('security', f'{p}: Chromium stable has {show(max(stable)[0])}, '
                                         f'newer than {show(best)} since {min(since)} '
                                         f'({(today - min(since)).days} days), and nothing '
                                         f'newer from {PRODUCTS[p][0]} fits milestone {target}'))
        # An exploit in the wild overrides the milestone rule: any upstream
        # release with the fix, or our own update when there is none.
        ours = current.get(p, best)
        for fixed, date, cves in zerodays:
            if ours >= fixed:
                continue
            fix = [r for r in rs if r[0] >= fixed]
            then = (f'{PRODUCTS[p][0]} {min(fix)[2]} has it: take it now' if fix
                    else f'{PRODUCTS[p][0]} has no release with it: update ourselves')
            findings.append(('zeroday', f'{p}: {", ".join(cves)} exploited in the wild, fixed '
                                        f'in Chromium {show(fixed)} ({date}); we build '
                                        f'{show(ours)}; {then}'))
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--desktop', help='an oganesson checkout')
    parser.add_argument('--android', help='a tennessine checkout')
    args = parser.parse_args()
    milestone = int((ROOT / 'milestone.txt').read_text())
    releases = {p: upstream_releases(name, repo, milestone)
                for p, (name, repo) in PRODUCTS.items()}
    stable = chromium_stable()
    titanium = github_releases('jqssun/android-titanium-browser')[0]
    today = datetime.date.today()
    fixes, unplaced = zero_days(today)
    findings = assess(milestone, releases, stable, current_versions(args.desktop, args.android),
                      today, fixes)
    findings += [('zeroday', f'{cve} is exploited in the wild (CISA KEV) but no recent Chrome '
                             f'Releases post names it: find the version that fixes it')
                 for cve in unplaced]
    print('status: action' if findings else 'status: current')
    for kind, text in findings:
        print(f'{kind}: {text}')
    print(f'info: milestone {milestone}; ' + '; '.join(
        f'{PRODUCTS[p][0]} {rs[0][2]} = Chromium {show(rs[0][0])} ({rs[0][1]})'
        for p, rs in releases.items())
        + f'; titanium {titanium[0]} ({titanium[1]}); '
        f'Linux stable {show(max(stable)[0])}')


if __name__ == '__main__':
    sys.exit(main())
