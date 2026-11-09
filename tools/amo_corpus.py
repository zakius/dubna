#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""The WebExtensions climb's corpus (webextensions/README.md): the 1000 add-ons
with the most users on addons.mozilla.org, as a fixed list.

  amo_corpus.py fetch [--refresh] [--cache DIR]
  amo_corpus.py scan --chromium SRC [--cache DIR]

fetch writes webextensions/corpus.json from AMO's v5 API when it is missing,
or with --refresh (once a quarter, in its own commit), then downloads each
add-on's .xpi into the cache and checks its SHA-256. A file already in the
cache with the right hash isn't downloaded again. The cache is outside the
repo: ~/.cache/dubna-amo-corpus by default.

scan reads each cached add-on's manifest and scripts and writes
webextensions/ranking.md: the manifest keys, permissions and browser.* APIs
the add-ons use that Chromium doesn't have (the _*_features.json files and
API schemas of the Chromium tree SRC), with the add-ons that use each.
"""
import argparse
import hashlib
import json
import re
import sys
import urllib.request
import zipfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORPUS = ROOT / 'webextensions/corpus.json'
RANKING = ROOT / 'webextensions/ranking.md'
CACHE = Path.home() / '.cache/dubna-amo-corpus'
SIZE = 1000
SEARCH = ('https://addons.mozilla.org/api/v5/addons/search/'
          '?type=extension&app=firefox&sort=users&page_size=50&page={}')


def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'dubna-amo-corpus'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def entry(result):
    """A corpus entry from one add-on of an AMO search result."""
    version = result['current_version']
    file = version['file']
    algorithm, _, digest = file['hash'].partition(':')
    if algorithm != 'sha256':
        raise ValueError(f"{result['guid']}: hash {file['hash']!r} isn't SHA-256")
    return {
        'id': result['guid'],
        'slug': result['slug'],
        'users': result['average_daily_users'],
        'version': version['version'],
        'xpi': file['url'],
        'sha256': digest,
    }


def entries(pages, size=SIZE):
    """The first `size` add-ons of the search result `pages`, in order. An
    add-on that moved to a later page while they were read appears once."""
    corpus = {}
    for page in pages:
        for result in page['results']:
            if result['guid'] not in corpus:
                corpus[result['guid']] = entry(result)
            if len(corpus) == size:
                return list(corpus.values())
    raise ValueError(f'AMO listed {len(corpus)} add-ons, not {size}')


def search_pages():
    page = 1
    while True:
        data = json.loads(get(SEARCH.format(page)))
        yield data
        if not data['next']:
            return
        page += 1


def cache_path(cache, item):
    return cache / f"{item['sha256']}.xpi"


def cached(cache, item):
    path = cache_path(cache, item)
    return path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256']


def download(cache, item):
    data = get(item['xpi'])
    if hashlib.sha256(data).hexdigest() != item['sha256']:
        raise ValueError(f"{item['id']}: {item['xpi']} doesn't match its SHA-256")
    path = cache_path(cache, item)
    partial = path.with_suffix('.part')
    partial.write_bytes(data)
    partial.replace(path)


def fetch(refresh, cache):
    if refresh or not CORPUS.exists():
        corpus = entries(search_pages())
        CORPUS.write_text(json.dumps(corpus, indent=1) + '\n')
        print(f'{CORPUS.relative_to(ROOT)}: {len(corpus)} add-ons')
    corpus = json.loads(CORPUS.read_text())
    cache.mkdir(parents=True, exist_ok=True)
    missing = [item for item in corpus if not cached(cache, item)]
    failed = []
    for number, item in enumerate(missing, 1):
        try:
            download(cache, item)
        except (OSError, ValueError) as error:
            failed.append(f"{item['id']}: {error}")
        if number % 50 == 0:
            print(f'{number} of {len(missing)} downloaded')
    print(f'{cache}: {len(corpus) - len(failed)} of {len(corpus)} .xpi files')
    for line in failed:
        print(line, file=sys.stderr)
    return 1 if failed else 0


# Scan.

API_DIRS = ('chrome/common/extensions/api', 'extensions/common/api')
# browser.<namespace>.<member>, or chrome.<namespace>.<member> for members;
# not a file name (polyfill/browser.js) or a property (result.browser.name).
API_CALL = re.compile(r'(?<![\w.$/-])(browser|chrome)\.([a-zA-Z]\w*)(?:\.([a-zA-Z]\w*))?')
# Firefox's WebExtensions namespaces (MDN's JavaScript APIs list): other
# names after browser. are an add-on's own objects (browser.name, a user
# agent parser's).
FIREFOX_APIS = {
    'action', 'alarms', 'bookmarks', 'browserAction', 'browserSettings', 'browsingData',
    'captivePortal', 'clipboard', 'commands', 'contentScripts', 'contextualIdentities',
    'cookies', 'declarativeContent', 'declarativeNetRequest', 'devtools', 'dns', 'downloads',
    'events', 'extension', 'extensionTypes', 'find', 'history', 'i18n', 'identity', 'idle',
    'management', 'menus', 'notifications', 'omnibox', 'pageAction', 'permissions', 'pkcs11',
    'privacy', 'proxy', 'runtime', 'scripting', 'search', 'sessions', 'sidebarAction',
    'storage', 'tabGroups', 'tabs', 'theme', 'topSites', 'trial', 'types', 'userScripts',
    'webNavigation', 'webRequest', 'windows',
}
# Firefox's metadata, which no add-on needs Chromium to read: the keys the
# corpus runner (oganesson's tools/e2e/corpus.py) ignores too.
IGNORED_KEYS = {'$schema', 'applications', 'browser_specific_settings', 'developer'}
OBJECT_MEMBERS = {'hasOwnProperty', 'toString', 'valueOf', 'constructor', 'prototype'}
SCRIPT = re.compile(r'\.(m?js|html?)$')
NAMES_SHOWN = 10


def read_features(path):
    """The feature names of a _*_features.json file, which has comments."""
    text = re.sub(r'("(?:\\.|[^"\\])*")|//[^\n]*|/\*.*?\*/', lambda m: m.group(1) or '',
                  path.read_text(), flags=re.DOTALL)
    return set(json.loads(text))


class Chromium:
    """What a Chromium tree's extensions system has: manifest keys,
    permissions, API namespaces and their schemas' text."""

    def __init__(self, src):
        self.keys, self.permissions, self.apis = set(), set(), set()
        self.schemas = {}
        for folder in API_DIRS:
            api = Path(src) / folder
            self.keys |= read_features(api / '_manifest_features.json')
            self.permissions |= read_features(api / '_permission_features.json')
            self.apis |= read_features(api / '_api_features.json')
            for schema in api.rglob('*'):
                if schema.suffix in ('.json', '.idl', '.webidl') and not schema.name.startswith('_'):
                    self.schemas[schema.stem] = self.schemas.get(schema.stem, '') + schema.read_text()

    def has_key(self, key):
        return key in self.keys

    def has_permission(self, permission):
        return permission in self.permissions

    def has_api(self, namespace):
        return namespace in self.apis or any(a.startswith(namespace + '.') for a in self.apis)

    def has_member(self, namespace, member):
        """Whether the namespace's schema names `member`. A namespace without
        a schema of that name (a sub-namespace such as devtools.panels) counts
        as having it."""
        if f'{namespace}.{member}' in self.apis:
            return True
        schema = self.schemas.get(re.sub(r'([A-Z])', r'_\1', namespace).lower())
        return schema is None or re.search(rf'\b{member}\b', schema) is not None


def host_pattern(permission):
    return '://' in permission or permission == '<all_urls>'


def uses(xpi):
    """An add-on's manifest keys, permissions and API names, as sets of
    'key:', 'permission:' and 'api:' strings."""
    found = set()
    with zipfile.ZipFile(xpi) as archive:
        manifest = json.loads(archive.read('manifest.json').decode('utf-8-sig'))
        found |= {f'key:{key}' for key in manifest if key not in IGNORED_KEYS}
        for list_key in ('permissions', 'optional_permissions'):
            for permission in manifest.get(list_key, []):
                if isinstance(permission, str) and not host_pattern(permission):
                    found.add(f'permission:{permission}')
        for name in archive.namelist():
            if not SCRIPT.search(name):
                continue
            text = archive.read(name).decode('utf-8', 'replace')
            for prefix, namespace, member in API_CALL.findall(text):
                if namespace not in FIREFOX_APIS:
                    continue
                if prefix == 'browser':
                    found.add(f'api:{namespace}')
                # Not a type (TypeScript's runtime.Manifest) or an object's own.
                if member and member[0].islower() and member not in OBJECT_MEMBERS:
                    found.add(f'api:{namespace}.{member}')
    return found


def missing(chromium, use):
    """Whether Chromium lacks a 'key:', 'permission:' or 'api:' use."""
    kind, _, name = use.partition(':')
    if kind == 'key':
        return not chromium.has_key(name)
    if kind == 'permission':
        return not chromium.has_permission(name)
    namespace, _, member = name.partition('.')
    if not member:
        return not chromium.has_api(namespace)
    return chromium.has_api(namespace) and not chromium.has_member(namespace, member)


def scan(corpus, cache, chromium):
    """Each missing use, with the corpus entries that have it."""
    found = defaultdict(list)
    for item in corpus:
        for use in uses(cache_path(cache, item)):
            if missing(chromium, use):
                found[use].append(item)
    return found


SECTIONS = (('key', 'Manifest keys'), ('permission', 'Permissions'),
            ('api', 'APIs'))


def ranking(found, total):
    """ranking.md's text: each section's missing uses, the most add-ons first."""
    lines = ['# What the corpus misses in Chromium', '',
             f'Written by `tools/amo_corpus.py scan` from the {total} add-ons of `corpus.json`. '
             'Each row is a manifest key, permission or `browser.*` API that add-ons use and '
             "Chromium doesn't have, with the number of add-ons that use it and the first "
             f'{NAMES_SHOWN} of them by users. The matching is approximate: APIs are found by '
             'name in the scripts, so a name in a comment or in code that never runs counts, '
             'an API reached another way (`const api = browser; api.menus`) does not, only '
             "Firefox's namespaces count, and a "
             "member counts as missing when its namespace's schema doesn't name it. Read an "
             'add-on before filing a step for it alone.', '']
    for kind, title in SECTIONS:
        rows = sorted(((use.partition(':')[2], items) for use, items in found.items()
                       if use.startswith(kind + ':')),
                      key=lambda row: (-len(row[1]), row[0]))
        lines += [f'## {title}', '', '| Name | Add-ons | Which |', '|---|---|---|']
        for name, items in rows:
            items = sorted(items, key=lambda item: -item['users'])
            names = ', '.join(item['slug'] for item in items[:NAMES_SHOWN])
            if len(items) > NAMES_SHOWN:
                names += f', and {len(items) - NAMES_SHOWN} more'
            lines.append(f'| `{name}` | {len(items)} | {names} |')
        lines.append('')
    return '\n'.join(lines)


def scan_command(cache, src):
    corpus = json.loads(CORPUS.read_text())
    found = scan(corpus, cache, Chromium(src))
    RANKING.write_text(ranking(found, len(corpus)))
    print(f'{RANKING.relative_to(ROOT)}: {len(found)} missing names')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    commands = parser.add_subparsers(dest='command', required=True)
    fetch_parser = commands.add_parser('fetch', help='the list and the .xpi cache')
    fetch_parser.add_argument('--refresh', action='store_true',
                              help='read the list from AMO again')
    fetch_parser.add_argument('--cache', type=Path, default=CACHE)
    scan_parser = commands.add_parser('scan', help="ranking.md: what the add-ons use that Chromium hasn't")
    scan_parser.add_argument('--chromium', type=Path, required=True,
                             help='a Chromium tree (src) at the browsers\' version')
    scan_parser.add_argument('--cache', type=Path, default=CACHE)
    args = parser.parse_args()
    if args.command == 'scan':
        return scan_command(args.cache, args.chromium)
    return fetch(args.refresh, args.cache)


if __name__ == '__main__':
    sys.exit(main())
