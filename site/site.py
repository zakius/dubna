#!/usr/bin/env python3
"""dubna's site (https://dubna.zakius.xyz): build, publish, and check whether it
needs an update.

  site/site.py build      everything.html from everything.html.in and items.json,
                          into site/www with the other pages
  site/site.py publish    build, then copy site/www to the server (PUBLISH_TARGET);
                          the server's windows/ folder is left alone
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
import json
import os
import re
import subprocess
import sys
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
PUBLISH_TARGET = os.environ.get('PUBLISH_TARGET', 'oracle:dubna/www/')
PAGES = ('index.html', 'install.html')


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


def build():
    www = SITE / 'www'
    www.mkdir(exist_ok=True)
    items = json.loads((SITE / 'items.json').read_text())
    data = json.dumps(items, separators=(',', ':'), ensure_ascii=False).replace('</', '<\\/')
    (www / 'everything.html').write_text(
        (SITE / 'everything.html.in').read_text().replace('__DATA__', data))
    for page in PAGES:
        text = (SITE / page).read_text()
        if page == 'index.html':
            text = re.sub(r'id="count">\d+ →', f'id="count">{len(items)} →', text)
        (www / page).write_text(text)


def publish():
    build()
    subprocess.run(['rsync', '-a', '--delete', '--exclude=windows/', f'{SITE}/www/',
                    PUBLISH_TARGET], check=True)


def mark_reviewed():
    (SITE / 'reviewed.json').write_text(json.dumps(
        {name: git(repo, 'rev-parse', 'HEAD') for name, repo in REPOS.items()}, indent=2) + '\n')


if __name__ == '__main__':
    commands = {'build': build, 'publish': publish, 'check': check, 'reviewed': mark_reviewed}
    if len(sys.argv) != 2 or sys.argv[1] not in commands:
        sys.exit(__doc__)
    commands[sys.argv[1]]()
