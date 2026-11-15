#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""amo_corpus.py without the network: AMO's search results become corpus
entries in order, the cache keeps only files with the right hash, and the
scan finds what two small add-ons use that a small Chromium tree lacks."""
import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from amo_corpus import Chromium, cached, entries, ranking, scan, without


def result(guid, users):
    return {
        'guid': guid,
        'slug': guid.split('@')[0],
        'average_daily_users': users,
        'current_version': {
            'version': '1.0',
            'file': {'url': f'https://example.org/{guid}.xpi', 'hash': 'sha256:ab12'},
        },
    }


class EntriesTest(unittest.TestCase):

    def test_fields(self):
        corpus = entries([{'results': [result('a@x', 30)]}], size=1)
        self.assertEqual(corpus, [{'id': 'a@x', 'slug': 'a', 'users': 30, 'version': '1.0',
                                   'xpi': 'https://example.org/a@x.xpi', 'sha256': 'ab12'}])

    def test_order_across_pages_and_once_each(self):
        pages = [{'results': [result('a@x', 30), result('b@x', 20)]},
                 # b moved down a page while the pages were read.
                 {'results': [result('b@x', 20), result('c@x', 10), result('d@x', 5)]}]
        self.assertEqual([e['id'] for e in entries(pages, size=3)], ['a@x', 'b@x', 'c@x'])

    def test_too_few(self):
        with self.assertRaises(ValueError):
            entries([{'results': [result('a@x', 30)]}], size=2)

    def test_other_hash(self):
        other = result('a@x', 30)
        other['current_version']['file']['hash'] = 'sha1:ab12'
        with self.assertRaises(ValueError):
            entries([{'results': [other]}], size=1)


class WithoutTest(unittest.TestCase):

    def test_leaves_out_by_id(self):
        corpus = [{'id': 'a@x'}, {'id': 'b@x'}, {'id': 'c@x'}]
        needs_account = [{'id': 'b@x', 'slug': 'b', 'needs': 'password manager'}]
        self.assertEqual(without(corpus, needs_account), [{'id': 'a@x'}, {'id': 'c@x'}])


class CacheTest(unittest.TestCase):

    def test_hash_decides(self):
        with tempfile.TemporaryDirectory() as folder:
            cache = Path(folder)
            data = b'xpi'
            item = {'sha256': hashlib.sha256(data).hexdigest()}
            self.assertFalse(cached(cache, item))
            (cache / f"{item['sha256']}.xpi").write_bytes(b'broken')
            self.assertFalse(cached(cache, item))
            (cache / f"{item['sha256']}.xpi").write_bytes(data)
            self.assertTrue(cached(cache, item))



def write_xpi(path, manifest, scripts):
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr('manifest.json', '\ufeff' + json.dumps(manifest))
        for name, text in scripts.items():
            archive.writestr(name, text)


class ScanTest(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        # A Chromium tree: features files with comments, and one schema.
        for folder, apis in (('chrome/common/extensions/api', '"tabs": {}, "contextMenus": {}'),
                             ('extensions/common/api', '"runtime": {}, "devtools.panels": {}')):
            api = root / 'src' / folder
            api.mkdir(parents=True)
            (api / '_api_features.json').write_text(f'// Comment.\n{{{apis}}}\n')
            (api / '_manifest_features.json').write_text(
                '{"name": {}, "permissions": {}, "background": {"a": "http://x" /* not a comment */}}')
            (api / '_permission_features.json').write_text(
                '{"tabs": {  // a trailing comment\n}}')
        (root / 'src/chrome/common/extensions/api/tabs.json').write_text(
            '[{"namespace": "tabs", "functions": [{"name": "query"}]}]')
        self.cache = root / 'cache'
        self.cache.mkdir()
        self.corpus = [{'slug': 'one', 'users': 10, 'sha256': 'a'},
                       {'slug': 'two', 'users': 20, 'sha256': 'b'}]
        write_xpi(self.cache / 'a.xpi',
                  {'name': 'One', 'background': {}, 'sidebar_action': {},
                   'permissions': ['tabs', 'menus', '<all_urls>', 'https://x.org/*']},
                  {'bg.js': 'browser.menus.create({}); browser.tabs.query({}).then(t => '
                            'browser.tabs.hide(t)); chrome.tabs.Tab; browser.runtime.getURL("")'})
        write_xpi(self.cache / 'b.xpi',
                  {'name': 'Two', 'permissions': ['menus']},
                  {'lib/browser.js': 'result.browser.name; browser.name;'
                                     ' chrome.devtools.panels.create(); browser.menus.remove()',
                   'icon.png': 'browser.theme'})
        self.chromium = Chromium(root / 'src')

    def tearDown(self):
        self.tmp.cleanup()

    def test_missing_names(self):
        found = scan(self.corpus, self.cache, self.chromium)
        self.assertEqual({use: [item['slug'] for item in items] for use, items in found.items()},
                         {'key:sidebar_action': ['one'], 'permission:menus': ['one', 'two'],
                          'api:menus': ['one', 'two'], 'api:tabs.hide': ['one']})

    def test_ranking(self):
        text = ranking(scan(self.corpus, self.cache, self.chromium), 2)
        self.assertIn('| `menus` | 2 | two, one |', text)
        self.assertLess(text.index('## Manifest keys'), text.index('`sidebar_action`'))
        self.assertLess(text.index('`menus` | 2'), text.index('`tabs.hide` | 1'))


if __name__ == '__main__':
    unittest.main()
