#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""firefox_gates.py finds what the patch would give every extension: each test breaks the
committed patch in one way and expects the check to name it."""
import unittest

from firefox_gates import PATCH, check, chromium_view

PATCH_TEXT = PATCH.read_text()


def broken(old, new):
    assert old in PATCH_TEXT, old
    return check(PATCH_TEXT.replace(old, new, 1))


class FirefoxGatesTest(unittest.TestCase):

    def test_committed_patch_passes(self):
        self.assertEqual(check(), [])

    def test_unmarked_property(self):
        problems = broken('"firstPartyDomain": {"firefox_only": true, ',
                          '"firstPartyDomain": {')
        self.assertTrue(any('cookies.json' in p and 'firstPartyDomain' in p for p in problems),
                        problems)

    def test_unmarked_function(self):
        problems = broken('"name": "getBrowserInfo",\n+        "firefox_only": true,',
                          '"name": "getBrowserInfo",')
        self.assertTrue(any('runtime.json' in p for p in problems), problems)

    def test_unflagged_feature(self):
        problems = broken('''+  "theme": {
+    "requires_delegated_availability_check": true,''', '''+  "theme": {''')
        self.assertTrue(any('"theme"' in p for p in problems), problems)

    def test_ungated_code(self):
        problems = broken('+  firefox_schema_view::SetResultFilter(&RemoveFirefoxOnlyResults);',
                          '+  firefox_schema_view::SetResultFilter(&RemoveFirefoxOnlyResults);\n'
                          '+}\n+void EnableSomethingForEveryone() {\n+  Enable();')
        self.assertEqual(problems, [], 'a hunk that names the gate passes as a whole')
        problems = broken('+        static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));',
                          '+        static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));')
        self.assertTrue(any('network_prediction_transformer.cc' in p and 'applies to every' in p
                            for p in problems), problems)
        self.assertTrue(any('no hunk matches' in p for p in problems), problems)

    def test_chromium_view(self):
        node = {'properties': {
            'a': {'type': 'string', 'optional': True, 'chromium': {'optional': None}},
            'b': {'type': 'integer', 'firefox_only': True}},
            'enum': ['x', {'name': 'y', 'firefox_only': True}]}
        self.assertEqual(chromium_view(node),
                         {'properties': {'a': {'type': 'string'}}, 'enum': ['x']})


if __name__ == '__main__':
    unittest.main()
