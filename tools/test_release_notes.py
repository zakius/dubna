#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""release_notes.py on a throwaway repo: the first release lists the latest
changes, a later one the changes since its tag, docs-only commits are left
out, and a release without changes says it's a rebuild."""
import subprocess
import tempfile
import unittest
from pathlib import Path

from release_notes import notes

SKIP = ['docs/', '*.md', '.github/']


class ReleaseNotesTest(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.repo = self.tmp.name
        self.git('init', '-q')

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args):
        subprocess.run(['git', '-C', self.repo, '-c', 'user.name=t', '-c', 'user.email=t@t',
                        *args], check=True, capture_output=True)

    def commit(self, path, subject):
        file = Path(self.repo, path)
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(subject)
        self.git('add', '-A')
        self.git('commit', '-q', '-m', subject)

    def test_first_release(self):
        self.commit('src/a.cc', 'Feature A')
        self.commit('docs/a.md', 'Docs: A')
        self.commit('src/b.cc', 'Feature B')
        text = notes(self.repo, SKIP, limit=1)
        self.assertIn('First release on GitHub', text)
        self.assertIn('Feature B', text)
        self.assertNotIn('Feature A', text)  # past the limit
        self.assertNotIn('Docs: A', text)

    def test_since_previous_release(self):
        self.commit('src/a.cc', 'Feature A')
        self.git('tag', 'v1')
        self.commit('README.md', 'README: wording')
        self.commit('.github/workflows/r.yml', 'CI')
        self.commit('src/b.cc', 'Feature B')
        text = notes(self.repo, SKIP, limit=20)
        self.assertTrue(text.startswith('Changes since v1:'))
        self.assertIn('Feature B', text)
        for left_out in ('Feature A', 'README', 'CI'):
            self.assertNotIn(left_out, text)

    def test_rebuild(self):
        self.commit('src/a.cc', 'Feature A')
        self.git('tag', 'v1')
        self.commit('docs/a.md', 'Docs: A')
        self.assertEqual(notes(self.repo, SKIP, limit=20),
                         'A rebuild of v1, without changes to the browser.')


if __name__ == '__main__':
    unittest.main()
