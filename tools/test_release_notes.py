#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""release_notes.py on a throwaway repo: the first release lists the latest
changes, a later one the changes since its tag, docs-only commits are left
out, a release without changes says it's a rebuild, and notes that touch
WebExtensions end with the Firefox test score."""
import json
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

    def git(self, *args, repo=None):
        subprocess.run(['git', '-C', repo or self.repo, '-c', 'user.name=t', '-c', 'user.email=t@t',
                        *args], check=True, capture_output=True)

    def commit(self, path, subject, repo=None):
        file = Path(repo or self.repo, path)
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(subject)
        self.git('add', '-A', repo=repo)
        self.git('commit', '-q', '-m', subject, repo=repo)

    def score(self):
        path = Path(self.tmp.name, 'score.json')
        path.write_text(json.dumps({'date': '2026-11-16', 'browser': 'oganesson 157.0.1',
                                    'passing': 3, 'ported': 13, 'total': 843}))
        return path

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

    def test_score_when_a_subject_names_webextensions(self):
        self.commit('src/a.cc', 'Feature A')
        self.git('tag', 'v1')
        self.commit('src/b.cc', 'Firefox add-ons: tabs.query')
        text = notes(self.repo, SKIP, limit=20, score=self.score())
        self.assertTrue(text.endswith("\n\nFirefox's WebExtensions tests: 3 of 843 pass "
                                      '(13 ported so far), on oganesson 157.0.1, 2026-11-16.'))

    def test_no_score_without_webextensions(self):
        self.commit('src/a.cc', 'Feature A')
        self.git('tag', 'v1')
        self.commit('src/b.cc', 'Feature B')
        self.commit('docs/c.md', 'WebExtensions climb: docs')  # left out of the notes
        text = notes(self.repo, SKIP, limit=20, score=self.score())
        self.assertNotIn('Firefox', text)

    def test_score_when_dubna_webextensions_changed(self):
        dubna = Path(self.repo, 'dubna')
        dubna.mkdir()
        dubna = str(dubna)
        self.git('init', '-q', repo=dubna)
        self.commit('src/a.cc', 'Feature A', repo=dubna)
        self.commit('README.md', 'Move dubna')
        self.git('tag', 'v1')
        self.commit('src/b.cc', 'Feature B', repo=dubna)
        self.commit('README.md', 'Move dubna: feature B')
        self.assertNotIn('Firefox', notes(self.repo, SKIP, limit=20, score=self.score()))
        self.git('tag', 'v2')
        self.commit('webextensions/score/x.json', 'Scores', repo=dubna)
        self.commit('README.md', 'Move dubna: tests ported')
        self.assertIn("Firefox's WebExtensions tests: 3 of 843",
                      notes(self.repo, SKIP, limit=20, score=self.score()))


if __name__ == '__main__':
    unittest.main()
