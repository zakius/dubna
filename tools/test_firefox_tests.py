#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""firefox_tests.py without the network: test files come from the manifests,
each gets its API and state, and a state already set stays."""
import tempfile
import unittest
from pathlib import Path

from firefox_tests import api, entry, not_applicable, test_files

KNOWN = {'tabs', 'runtime', 'cookies', 'contextualIdentities', 'devtools.inspectedWindow',
         'declarativeNetRequest', 'storage', 'menus'}


class TestFilesTest(unittest.TestCase):

    def test_manifest_entries(self):
        with tempfile.TemporaryDirectory() as source:
            folder = Path(source) / 'toolkit/components/extensions/test/xpcshell'
            folder.mkdir(parents=True)
            (folder / 'xpcshell.toml').write_text(
                '[DEFAULT]\nhead = "head.js"\n\n["include:xpcshell-common.toml"]\n\n'
                '["test_ext_b.js"]\nskip-if = ["os == \'android\'"]\n\n["test_ext_a.js"]\n')
            (folder / 'xpcshell-common.toml').write_text('["test_ext_a.js"]\n')
            Path(source, 'browser/components/extensions/test').mkdir(parents=True)
            self.assertEqual(test_files(Path(source)),
                             ['toolkit/components/extensions/test/xpcshell/test_ext_a.js',
                              'toolkit/components/extensions/test/xpcshell/test_ext_b.js'])


class ApiTest(unittest.TestCase):

    def test_from_the_name(self):
        self.assertEqual(api('x/test_ext_cookies_firstParty.js', '', KNOWN), 'cookies')
        self.assertEqual(api('x/test_ext_contextual_identities.js', '', KNOWN),
                         'contextualIdentities')
        # The longest namespace that the name starts with.
        self.assertEqual(api('x/browser_ext_devtools_inspectedWindow_eval.js', '',
                             KNOWN | {'devtools'}), 'devtools.inspectedWindow')
        self.assertEqual(api('x/test_ext_dnr_api.js', '', KNOWN), 'declarativeNetRequest')
        self.assertEqual(api('x/test_ext_contentscript_css.js', '', KNOWN), 'content_scripts')
        # Whole words: contentscript_slow is a manifest content script.
        self.assertEqual(api('x/test_ext_contentscript_slow_frame.js', '', KNOWN | {'contentScripts'}),
                         'content_scripts')
        self.assertEqual(api('x/test_ext_contentScripts_register.js', '', KNOWN | {'contentScripts'}),
                         'contentScripts')

    def test_from_the_calls(self):
        text = 'browser.test.assertEq(1, 1); browser.tabs.query({}); browser.tabs.get(1);'
        self.assertEqual(api('x/test_ext_incognito.js', text + 'browser.runtime.id', KNOWN), 'tabs')
        self.assertEqual(api('x/test_ext_incognito.js', 'browser.test.log("x")', KNOWN), '')
        # A namespace's calls add up, with those of its events.
        text = ('browser.runtime.onMessage.addListener(); browser.runtime.sendMessage();'
                'browser.runtime.connect(); browser.tabs.query(); browser.tabs.get();')
        self.assertEqual(api('x/test_ext_incognito.js', text, KNOWN), 'runtime')


class StateTest(unittest.TestCase):

    def test_not_applicable(self):
        self.assertIn('telemetry', not_applicable('x/test_ext_storage_telemetry.js'))
        self.assertIn('WebIDL', not_applicable('x/webidl-api/test_ext_webidl_api.js'))
        self.assertIsNone(not_applicable('x/test_ext_storage_session.js'))
        # The Schemas module's own tests, but not those that load add-ons.
        self.assertIsNotNone(not_applicable('x/test_ext_schemas_async.js'))
        self.assertIsNone(not_applicable('x/test_ext_schemas_versioned.js'))

    def test_new_entries(self):
        self.assertEqual(entry('x/test_ext_storage_session.js', '', KNOWN, None),
                         {'path': 'x/test_ext_storage_session.js', 'api': 'storage',
                          'state': 'todo'})
        self.assertEqual(entry('x/test_ext_telemetry.js', '', KNOWN, None)['state'], 'n/a')

    def test_kept_states(self):
        ported = {'path': 'x/test_ext_storage_session.js', 'api': 'old', 'state': 'ported',
                  'port': 'firefox/test_ext_storage_session.py'}
        self.assertEqual(entry('x/test_ext_storage_session.js', '', KNOWN, ported),
                         dict(ported, api='storage'))
        by_hand = {'path': 'x/test_ext_tabs_lazy.js', 'api': 'tabs', 'state': 'n/a',
                   'reason': 'set by hand'}
        self.assertEqual(entry('x/test_ext_tabs_lazy.js', '', KNOWN, by_hand), by_hand)
        # A todo entry is classified again.
        todo = {'path': 'x/test_ext_telemetry.js', 'api': '', 'state': 'todo'}
        self.assertEqual(entry('x/test_ext_telemetry.js', '', KNOWN, todo)['state'], 'n/a')
        # So is an n/a that a rule made, which no rule matches any more.
        by_rule = {'path': 'x/test_ext_tabs_lazy.js', 'api': 'tabs', 'state': 'n/a',
                   'reason': "Gecko's process and background modes, set by prefs"}
        self.assertEqual(entry('x/test_ext_tabs_lazy.js', '', KNOWN, by_rule)['state'], 'todo')


if __name__ == '__main__':
    unittest.main()
