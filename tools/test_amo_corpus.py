#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""amo_corpus.py without the network: AMO's search results become corpus
entries in order, and the cache keeps only files with the right hash."""
import hashlib
import tempfile
import unittest
from pathlib import Path

from amo_corpus import cached, entries


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


if __name__ == '__main__':
    unittest.main()
