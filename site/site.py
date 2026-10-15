#!/usr/bin/env python3
"""dubna's site (https://dubna.zakius.xyz): build, publish, and check whether it
needs an update.

  site/site.py build      everything.html from everything.html.in and items.json,
                          into site/www with the other pages and site/img; the
                          pages' {{name}}s get the current versions, dates and
                          minimum systems (facts())
  site/site.py screenshots  site/img from oganesson's and tennessine's tools/screenshots.py
  site/site.py publish    build, then copy site/www to PUBLISH_TARGET (HOST:DIR, from .env),
                          and server/compose.yaml and nginx.conf next to it, then
                          restarts what changed; the server's windows/ folder
                          (windows/publish.sh in oganesson) is left alone
  site/site.py check      what changed in our repos since the site was last
                          reviewed (reviewed.json): patches added to or removed
                          from a series but not in items.json or still in it, and
                          commits touching what users see; prints nothing when
                          the site is current
  site/site.py reviewed   record the repos' current commits in reviewed.json,
                          after updating the site

The repos are found next to dubna (../oganesson, ../tennessine), or through
OGANESSON and TENNESSINE. Only our changes are checked: Helium's series (we move
it), our own patches and docs. Titanium and Vanadium are summarized on the site,
not listed patch by patch.
"""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

SITE = Path(__file__).resolve().parent
DUBNA = SITE.parent
REPOS = {
    'dubna': DUBNA,
    'oganesson': Path(os.environ.get('OGANESSON', DUBNA.parent / 'oganesson')),
    'tennessine': Path(os.environ.get('TENNESSINE', DUBNA.parent / 'tennessine')),
}
# What users see, per repo: a commit touching these may need a site update.
WATCHED = {
    'dubna': ['patches', 'src', 'rust', 'README.md'],
    'oganesson': ['patches', 'src', 'late', 'linux/late', 'windows/late', 'README.md',
                  'goals.md', 'docs', 'helium', 'linux/helium-linux', 'windows/helium-windows'],
    'tennessine': ['patches', 'src', 'README.md', 'docs'],
}
PAGES = ('index.html', 'install.html')


def env(name):
    """NAME from the environment, else from dubna's .env, which Git ignores: where things
    are deployed stays out of the repository."""
    if name in os.environ:
        return os.environ[name]
    path = DUBNA / '.env'
    for line in path.read_text().splitlines() if path.exists() else []:
        key, _, value = line.partition('=')
        if key.strip() == name:
            return value.strip()
    sys.exit(f'site.py: set {name} in the environment or in {path}, e.g. {name}=myserver:dubna/www/')


def series(path, prefix=''):
    """Entries of a quilt series file, without .patch, with PREFIX."""
    if not path.exists():
        return []
    out = []
    for line in path.read_text().splitlines():
        line = line.split('#')[0].strip()
        if line:
            out.append(prefix + re.sub(r'\.patch$', '', line))
    return out


def ours():
    """Every patch the site lists one by one, in the repos' current state."""
    og, ts = REPOS['oganesson'], REPOS['tennessine']
    names = series(og / 'helium/patches/series')
    for entry in series(og / 'patches/series'):
        names.append(entry.removeprefix('../'))
    for late in ('late', 'linux/late', 'windows/late'):
        names += series(og / late / 'series', late + '/')
    for entry in series(ts / 'patches/series'):
        if entry.startswith('patches/tennessine/'):
            names.append(entry.removeprefix('patches/'))
        elif entry.startswith('dubna/'):
            names.append(entry)
    return set(names)


def listed(items):
    paths = set()
    for item in items:
        for path in item['p'].split(' · '):
            paths.add(path)
    return paths


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True,
                          capture_output=True, text=True).stdout.strip()


def check():
    items = json.loads((SITE / 'items.json').read_text())
    reviewed = json.loads((SITE / 'reviewed.json').read_text())
    report = []
    current, shown = ours(), listed(items)
    new = sorted(current - shown)
    gone = sorted(p for p in shown & {i['p'] for i in items if '/' in i['p']}
                  if p not in current and is_patch(p))
    if new:
        report.append('Patches not on the site:\n' + ''.join(f'  {p}\n' for p in new))
    if gone:
        report.append('Patches on the site but no longer in a series:\n'
                      + ''.join(f'  {p}\n' for p in gone))
    for name, repo in REPOS.items():
        log = git(repo, 'log', '--format=  %h %s', f'{reviewed[name]}..HEAD', '--',
                  *WATCHED[name])
        if log:
            report.append(f'{name}: commits since the site was reviewed:\n{log}\n')
    print('\n'.join(report), end='')


def is_patch(path):
    """Whether a site path names a patch in a series (not a source folder or a
    summary item)."""
    return not path.endswith('/') and not path.startswith(('vanadium', 'titanium/', 'src/',
                                                           'tools/', 'README', 'dubna/README'))


# everything.html's areas, in page order, with their blurbs.
AREAS = [
    ('Sync', "Firefox-compatible Sync in both browsers, with your own sync server if you like. Helium's dormant sync vault is under Build & plumbing."),
    ('Extensions & uBlock', 'Full uBlock Origin, Manifest V2, Web Store and AMO installs, Chrome extensions on Android.'),
    ('Tabs & windows', 'Firefox tab placement and selection, recently-used Ctrl+Tab, tab menus, split view, web apps.'),
    ('Address bar & search', 'Search engines, suggestions, the address bar itself.'),
    ('Input & shortcuts', 'Mouse gestures, editable keyboard shortcuts, context menus, zoom.'),
    ('History', 'Switches that stop saving history.'),
    ('Media', 'Autoplay and tab sound indicators.'),
    ('De-Googling & telemetry', 'Connections to Google and telemetry, cut. Useful Google-backed features come back as opt-ins.'),
    ('Fingerprinting', "Fingerprinting defenses: mostly flags, Helium's noise off by default."),
    ('Network & security', 'HTTPS-first, certificate transparency, WebRTC, DNS, TLS and network flags.'),
    ('Helium services', "Helium's own servers (proxy, bangs, updates, crash reports): carried, off by default."),
    ('Importers', 'Import from other browsers.'),
    ('Settings & flags', 'Settings pages, chrome://flags entries and about:config.'),
    ('Look & layout', "Helium's redesign and layouts, toolbars, Android's bottom-first layout."),
    ('Platform & packaging', 'Linux, Windows and Android builds, repositories, profiles.'),
    ('Build & plumbing', 'Build fixes and groundwork other patches need.'),
]
# Item states: label and colour. The page's script filters on these keys.
STATES = {'on': ('on', 'var(--on)'), 'flag': ('flag', 'var(--flag)'),
          'off': ('off by default', 'var(--off)'), 'dropped': ('dropped', 'var(--drop)')}
BROWSER = {'og': 'oganesson', 'ts': 'tennessine'}


# Words in patch names that read wrong once the name becomes a heading.
WORDS = {'ublock': 'uBlock', 'mru': 'MRU', 'ntp': 'NTP', 'webrtc': 'WebRTC', 'webstore': 'Web Store',
         'pdf': 'PDF', 'pwa': 'PWA', 'rbe': 'RBE', 'jit': 'JIT', 'ext': 'extension', 'dont': "don't",
         'mv2': 'MV2', 'ui': 'UI', 'ua': 'UA', 'url': 'URL', 'urls': 'URLs', 'api': 'API', 'gcm': 'GCM',
         'hsts': 'HSTS', 'http': 'HTTP', 'https': 'HTTPS', 'id': 'ID', 'js': 'JS', 'tls': 'TLS', 'ai': 'AI',
         'devtools': 'DevTools', 'webrequest': 'webRequest', 'webapp': 'web app', 'x86': 'x86',
         'reenable': 're-enable', 'degoogle': 'de-Google', 'nativelink': 'NativeLink', 'chrome': 'Chrome'}


def title(p):
    """An item's heading, from its patch path or project: name."""
    if p.startswith('project:'):
        t = p[8:].replace('-', ' ')
    elif ' ' in p:
        return p
    else:
        t = (re.sub(r'\.patch$', '', p.rstrip('/').split('/')[-1]) or p).replace('-', ' ')
    t = ' '.join(WORDS.get(w.lower(), w) for w in t.split())
    return t if t.startswith(('uBlock', 'webRequest')) else t[:1].upper() + t[1:]


def everything(items):
    """everything.html's stats, filter chips, contents and list, in full so the
    page reads without JS."""
    e = html.escape
    count = lambda f: sum(1 for d in items if f(d))
    stats = ''.join(f'<div class="stat"><b>{n}</b><span>{l}</span></div>' for n, l in [
        (count(lambda d: d['b'] == 'og' and not d.get('own')), 'Helium-series patches'),
        (count(lambda d: d['b'] == 'og' and d.get('own')), 'oganesson items'),
        (count(lambda d: d['b'] == 'both'), 'shared (dubna)'),
        (count(lambda d: d['b'] == 'ts'),
         f"tennessine items, {count(lambda d: d['b'] == 'ts' and d.get('own'))} mine"),
        (count(lambda d: d['s'] == 'flag'), 'behind flags'),
        (count(lambda d: 'drop' in d['s']), 'dropped')])
    chips = ''.join(f'<button class="chipf" data-k="{k}" aria-pressed="true"><i style="background:{c}"></i>{l}</button>'
                    for k, (l, c) in STATES.items())
    toc = main = ''
    for area, blurb in AREAS:
        xs = sorted((d for d in items if d['a'] == area), key=lambda d: (not d.get('own'), d['b']))
        if not xs:
            continue
        aid = re.sub(r'\W+', '-', area).lower()
        toc += f'<a href="#{aid}">{e(area)}<span>{len(xs)}</span></a>'
        main += f'<section class="cat" id="{aid}"><h2>{e(area)}<small>{len(xs)}</small></h2><p>{e(blurb)}</p><div class="list">'
        for d in xs:
            t = d.get('t') or title(d['p'])
            key = ' '.join([t, d['p'], d['w'], d['o'], d['n'], d['a']]).lower()
            badges = '<span class="b own">mine</span>' if d.get('own') else ''
            badges += ''.join(f'<span class="b {b}">{BROWSER[b]}</span>' for b in (('og', 'ts') if d['b'] == 'both' else (d['b'],)))
            badges += '' if d.get('own') else f'<span class="b">{e(d["o"])}</span>'
            badges += f'<span class="s {d["s"].replace(" ", "-")}">{STATES[d["s"]][0]}</span>'
            main += (f'\n<div class="it" data-b="{d["b"]}" data-s="{d["s"]}"{" data-own=1" if d.get("own") else ""} data-k="{e(key)}">'
                     f'<div class="t">{e(t)}</div><div class="meta">{badges}</div>'
                     + (f'<div class="w">{e(d["w"])}</div>' if d['w'] else '')
                     + (f'<div class="n">{e(d["n"])}</div>' if d['n'] else '')
                     + ('' if d['p'].startswith('project:') or '/' not in d['p'] else f'<div class="path">{e(d["p"])}</div>')
                     + '</div>')
        main += '</div></section>\n'
    missing = {d['a'] for d in items} - {a for a, _ in AREAS}
    assert not missing, f'items.json areas not in AREAS: {missing}'
    return {'__STATS__': stats, '__CHIPS__': chips, '__TOC__': toc, '__MAIN__': main}


# Android API levels to the release a user knows (developer.android.com/tools/releases/platforms).
ANDROID = {29: '10', 30: '11', 31: '12', 32: '12L', 33: '13', 34: '14', 35: '15', 36: '16',
           37: '17'}
MONTHS = 'Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec'.split()
FDROID_INDEX = Path.home() / '.local/share/tennessine-fdroid/repo/index-v1.json'


def facts():
    """What the pages say about the current builds: versions, dates, Chromium
    milestones and minimum systems, read from the newest packages (what was
    last published) and the build configuration, so the pages can't drift."""
    og, ts = REPOS['oganesson'], REPOS['tennessine']
    day = lambda stamp: f'{int(stamp[6:8])} {MONTHS[int(stamp[4:6]) - 1]} {stamp[:4]}'

    def newest(folder):
        """VERSION and build date of the newest VERSION-1.TIME package folder."""
        names = sorted((d.name for d in folder.glob('[0-9]*-1.[0-9]*') if d.is_dir()),
                       key=lambda n: n.split('-1.')[1])
        if not names:
            sys.exit(f'site.py: no packages in {folder}')
        version, stamp = names[-1].split('-1.')
        return version, day(stamp)

    og_version, og_date = newest(og / 'work/dl')
    win_version, win_date = newest(og / 'work/dl/windows')
    # Chromium's version, or, in packages before it, Helium's: its own, then the
    # Chromium milestone minus 136.
    def milestone(version):
        major, minor = version.split('.')[:2]
        return major if int(major) >= 100 else str(136 + int(minor))
    apks = json.loads(FDROID_INDEX.read_text())['packages']['io.github.zakius.tennessine']
    apk = max(apks, key=lambda a: a['versionCode'])
    added = time.strftime('%Y%m%d', time.gmtime(apk['added'] / 1000))
    deb = next((og / 'work/dl/latest').glob('oganesson_*.deb'))
    depends = subprocess.run(['dpkg-deb', '-f', str(deb), 'Depends'], check=True,
                             capture_output=True, text=True).stdout
    nsi = (og / 'windows/helium-windows/installer/helium.nsi').read_text()
    return {
        'og_version': og_version, 'og_date': og_date, 'og_milestone': milestone(og_version),
        'win_version': win_version, 'win_date': win_date, 'win_milestone': milestone(win_version),
        'ts_version': apk['versionName'], 'ts_date': day(added),
        'ts_milestone': apk['versionName'].split('.')[0],
        'milestone': (DUBNA / 'milestone.txt').read_text().strip(),
        'linux_cpu': re.search(r'x86-64-v\d', (og / 'linux/container-build.sh').read_text()).group(),
        # The time64 package names ("t64") came with Debian 13 and Ubuntu 24.04.
        'linux_distros': 'Debian 13, Ubuntu 24.04 or newer' if 't64' in depends
                         else 'Debian 12, Ubuntu 22.04 or newer',
        'windows_min': re.search(r'AtLeastWin(\d+)', nsi).group(1),
        'android_min': ANDROID[apk['minSdkVersion']],
    }


def fill(text, values):
    """Replaces each {{name}} in TEXT with its value."""
    return re.sub(r'\{\{(\w+)\}\}', lambda m: html.escape(values[m.group(1)]), text)


def build():
    www = SITE / 'www'
    www.mkdir(exist_ok=True)
    items = json.loads((SITE / 'items.json').read_text())
    page = (SITE / 'everything.html.in').read_text()
    for k, v in everything(items).items():
        page = page.replace(k, v)
    (www / 'everything.html').write_text(page)
    values = facts()
    for page in PAGES:
        text = fill((SITE / page).read_text(), values)
        if page == 'index.html':
            text = re.sub(r'id="count">\d+ →', f'id="count">{len(items)} →', text)
        (www / page).write_text(text)
    shutil.copytree(SITE / 'img', www / 'img', dirs_exist_ok=True)


def screenshots():
    """site/img from oganesson's tools/screenshots.py, with the installed package
    unless OGANESSON_BINARY says otherwise, and tennessine's on the emulator."""
    og = REPOS['oganesson']
    environ = dict(os.environ, TMPDIR=str(og / 'work/tmp'))
    environ.setdefault('OGANESSON_BINARY', '/opt/oganesson/oganesson')
    subprocess.run([sys.executable, str(og / 'tools/screenshots.py'), str(SITE / 'img')],
                   env=environ, check=True)
    # tennessine's on the Android emulator, from its newest x86_64 build.
    ts = REPOS['tennessine']
    apks = sorted((ts / 'work/build/tennessine/chromium/src/out/release').glob('*-x86_64.apk'),
                  key=lambda f: f.stat().st_mtime)
    if not apks:
        sys.exit('site.py: no x86_64 APK; build one with ./tennessine.py build x64')
    subprocess.run([sys.executable, str(ts / 'tools/screenshots.py'), str(apks[-1]),
                    str(SITE / 'img')], check=True)


def publish():
    build()
    target = env('PUBLISH_TARGET')
    subprocess.run(['rsync', '-a', '--delete', '--exclude=windows/', f'{SITE}/www/',
                    target], check=True)
    # The server's compose project and nginx config sit next to www/.
    host, path = target.split(':', 1)
    project = os.path.dirname(path.rstrip('/')) or '.'
    subprocess.run(['rsync', '-a', f'{SITE}/server/compose.yaml', f'{SITE}/server/nginx.conf',
                    f'{host}:{project}/'], check=True)
    subprocess.run(['ssh', host, f'cd {project} && docker compose up -d --quiet-pull'], check=True)


def mark_reviewed():
    (SITE / 'reviewed.json').write_text(json.dumps(
        {name: git(repo, 'rev-parse', 'HEAD') for name, repo in REPOS.items()}, indent=2) + '\n')


if __name__ == '__main__':
    commands = {'build': build, 'publish': publish, 'screenshots': screenshots, 'check': check,
                'reviewed': mark_reviewed}
    if len(sys.argv) != 2 or sys.argv[1] not in commands:
        sys.exit(__doc__)
    commands[sys.argv[1]]()
