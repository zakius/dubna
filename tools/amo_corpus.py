#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""The WebExtensions climb's corpus (webextensions/README.md): the 1000 add-ons
with the most users on addons.mozilla.org, as a fixed list.

  amo_corpus.py fetch [--refresh] [--cache DIR]

fetch writes webextensions/corpus.json from AMO's v5 API when it is missing,
or with --refresh (once a quarter, in its own commit), then downloads each
add-on's .xpi into the cache and checks its SHA-256. A file already in the
cache with the right hash isn't downloaded again. The cache is outside the
repo: ~/.cache/dubna-amo-corpus by default.
"""
import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORPUS = ROOT / 'webextensions/corpus.json'
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


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    commands = parser.add_subparsers(dest='command', required=True)
    fetch_parser = commands.add_parser('fetch', help='the list and the .xpi cache')
    fetch_parser.add_argument('--refresh', action='store_true',
                              help='read the list from AMO again')
    fetch_parser.add_argument('--cache', type=Path, default=CACHE)
    args = parser.parse_args()
    return fetch(args.refresh, args.cache)


if __name__ == '__main__':
    sys.exit(main())
