#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""site.py's release notes without the network: notes become escaped paragraphs and
lists, and a feed is valid RSS that links each release to its card."""
import importlib.util
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

# Loaded by path: "import site" finds Python's own site module.
spec = importlib.util.spec_from_file_location('site_py', Path(__file__).with_name('site.py'))
site_py = importlib.util.module_from_spec(spec)
spec.loader.exec_module(site_py)
feed, notes_html, release_list = site_py.feed, site_py.notes_html, site_py.release_list

NOTES = '''Changes since v1:

- Tabs: <b> & friends (05a545c)
- Sync: no hash here

Firefox add-ons corpus score: L2 861 → 882.
'''
RELEASE = {'b': 'og', 'tag': 'v2', 'title': 'oganesson 2 (1.2)', 'date': '2026-11-15T14:23:31Z',
           'notes': NOTES}


class SiteTest(unittest.TestCase):

    def test_notes(self):
        self.assertEqual(notes_html(NOTES),
                         '<p>Changes since v1:</p>'
                         '<ul><li>Tabs: &lt;b&gt; &amp; friends <code>05a545c</code></li>'
                         '<li>Sync: no hash here</li></ul>'
                         '<p>Firefox add-ons corpus score: L2 861 → 882.</p>')

    def test_feed(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'releases.xml'
            feed(path, 'oganesson releases', [RELEASE])
            item = ET.parse(path).getroot().find('channel/item')
        link = 'https://dubna.zakius.xyz/releases.html#oganesson-v2'
        self.assertEqual(item.find('link').text, link)
        self.assertEqual(item.find('guid').text, link)
        self.assertEqual(item.find('pubDate').text, 'Sun, 15 Nov 2026 14:23:31 GMT')
        self.assertEqual(item.find('description').text, notes_html(NOTES))
        self.assertIn('id="oganesson-v2"', release_list([RELEASE]))


if __name__ == '__main__':
    unittest.main()
