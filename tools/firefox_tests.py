#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""The WebExtensions climb's test inventory (webextensions/README.md, "Test
score"): every WebExtensions test file in Firefox's source, with its state.

  firefox_tests.py [--source DIR] [--no-fetch]

It reads the test manifests (*.toml) under toolkit/components/extensions/test/
and browser/components/extensions/test/ of a sparse, shallow clone of
Firefox's GitHub mirror, made or updated in the cache outside the repo
(~/.cache/dubna-firefox-tests/src by default; --no-fetch reads it as it is),
and writes webextensions/firefox-tests.json: each test file's path, the API it
tests and its state.

- `ported`: it runs in oganesson's tools/e2e (the port's file is `port`).
- `todo`: not ported yet.
- `n/a`: it tests a Gecko internal, not WebExtensions behaviour; `reason`
  says which. NOT_APPLICABLE sets these, or a hand-written entry with
  `"by_hand": true`.

A run keeps a file that is `ported` (with its `port`) or set by hand. Every
other file follows NOT_APPLICABLE again, so a changed rule changes its files. A file that is no
longer in Firefox's tree is left out, and the run names it.

The API is the namespace that the file name's first words name (ALIASES for
Firefox's own short names), or else the namespace the file calls most
(`browser.<ns>.` and `browser.<ns>.<sub>.`), or else "".
"""
import argparse
import json
import re
import subprocess
import sys
import tomllib
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / 'webextensions/firefox-tests.json'
SOURCE = Path.home() / '.cache/dubna-firefox-tests/src'
REPOSITORY = 'https://github.com/mozilla-firefox/firefox.git'
TEST_DIRS = ('toolkit/components/extensions/test', 'browser/components/extensions/test')
SCHEMA_DIRS = ('toolkit/components/extensions/schemas', 'browser/components/extensions/schemas')
# Firefox's short names in test file names, and the manifest keys that tests
# are named after.
ALIASES = {
    'dnr': 'declarativeNetRequest',
    'contentscript': 'content_scripts',
    'background': 'background',
    'themes': 'theme',
    'webnavigation': 'webNavigation',
    'webrequest': 'webRequest',
    'eventpage': 'background',
    'popup': 'browserAction',
    'optionspage': 'options_ui',
    'manifest': 'manifest',
    'permission': 'permissions',
    'settings_overrides': 'chrome_settings_overrides',
    'url_overrides': 'chrome_url_overrides',
    'sendmessage': 'runtime',
    'unlimitedstorage': 'unlimitedStorage',
    'web_accessible': 'web_accessible_resources',
    'userscripts': 'userScripts',
}
# Test files that test Gecko, not WebExtensions behaviour: a pattern of the
# path, and the reason.
NOT_APPLICABLE = [
    (r'/marionette/', "Gecko's stored service worker registrations, through Marionette"),
    (r'/webidl-api/|_webidl_', "Gecko's WebIDL bindings of the extension APIs"),
    (r'-remote-false/|remote_mode|_verify_sw_mode|backgroundServiceWorker_enabled_pref'
     r'|serviceworker_pref_disabled', "Gecko's process and background modes, set by prefs"),
    (r'process_crash|process_alive', "Gecko's extension process and its crash observer"),
    (r'telemetry|_glean', "Firefox's telemetry, which only Mozilla's own code reads"),
    (r'experiment', "experiment_apis, privileged Gecko code that only Mozilla-signed add-ons run"),
    (r'geckoProfiler', "geckoProfiler, a privileged API for Mozilla's profiler"),
    (r'startup_?[cC]ache|startupcache|startupData|startup_canary|startup_perf'
     r'|reload_manifest_cache', "Gecko's startup cache and startup internals"),
    (r'test_ChannelWrapper', "Gecko's ChannelWrapper class, under webRequest"),
    (r'test_ExtensionShortcutKeyMap', "Gecko's internal map of shortcut keys"),
    (r'test_ExtensionTaskScheduler', "Gecko's internal task scheduler"),
    (r'test_GuardSets', "Gecko's enterprise permission guards, called directly"),
    (r'test_MatchPattern', "Gecko's MatchPattern class; the APIs that take patterns test them"),
    (r'test_StorageSyncService|storage_sync_kinto|storage_content_sync_kinto'
     r'|storage_idb_data_migration|storage_local_corrupted_idb',
     "Gecko's storage backends and their migration"),
    (r'test_WebExtensionContentScript|test_WebExtensionPolicy', "Gecko's WebExtensionPolicy classes"),
    (r'test_csp_validator', "Gecko's CSP validator, called directly; manifests test it as add-ons see it"),
    (r'test_extensionUtils|_ext_contexts\.js|MessageManagerProxy|extensionPreferencesManager'
     r'|extensionSettingsStore|_debugging_utils|_load_all_api_modules',
     "Gecko's extension modules, called directly"),
    (r'_ext_schemas(_async|_allowed_contexts|_revoke|_roots)?\.js',
     "Gecko's Schemas module, with made-up schemas"),
    (r'contexts_gc|ports_gc|shutdown_cleanup', "Gecko's garbage collection of its own objects"),
    (r'test_extension_permissions_migrat', "Gecko's permission store migration"),
    (r'test_locale_converter|test_locale_data', "Gecko's locale classes, called directly"),
    (r'QuarantinedDomains', "Mozilla's quarantined domains, set by Mozilla's remote settings"),
    (r'OriginControls\.js|originControls_internals', "Firefox's OriginControls module, called directly"),
    (r'resistfingerprinting|resistFingerprinting', "Gecko's resist-fingerprinting mode"),
    (r'test_wpt_', "Gecko's web-platform-tests support"),
    (r'trustworthy_origin', "Gecko's security manager on moz-extension: URLs, called directly"),
    (r'activityLog', "Firefox's activity log for its own debugging tools"),
    (r'_ext_test\.html|_ext_test_mock|_ext_test_wrapper', 'browser.test and the test harness itself'),
    (r'browser_unified_extensions|browser_AMBrowserExtensionsImport'
     r'|browser_ExtensionControlledPopup|browser_always_on_pbm_prompt',
     "Firefox's own extensions UI, not add-on behaviour"),
]


def fetch(source):
    """Clones Firefox's test and schema folders into `source`, sparse and
    shallow, or brings the clone up to date."""
    if not (source / '.git').is_dir():
        source.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'clone', '--depth', '1', '--filter=blob:none', '--sparse',
                        REPOSITORY, str(source)], check=True)
    else:
        subprocess.run(['git', '-C', str(source), 'fetch', '--depth', '1', 'origin'], check=True)
        subprocess.run(['git', '-C', str(source), 'reset', '-q', '--hard', 'FETCH_HEAD'], check=True)
    subprocess.run(['git', '-C', str(source), 'sparse-checkout', 'set', *TEST_DIRS, *SCHEMA_DIRS],
                   check=True)


def test_files(source):
    """The test files that the manifests under TEST_DIRS list, as paths from
    `source`, sorted."""
    found = set()
    for folder in TEST_DIRS:
        for manifest in (source / folder).rglob('*.toml'):
            for name in tomllib.loads(manifest.read_text()):
                if name != 'DEFAULT' and not name.startswith('include:'):
                    found.add((manifest.parent / name).relative_to(source).as_posix())
    return sorted(found)


def namespaces(source):
    """The API namespaces of Firefox's schemas."""
    found = set()
    for folder in SCHEMA_DIRS:
        for schema in (source / folder).glob('*.json'):
            # Not json.loads(): the files have comments.
            found.update(re.findall(r'"namespace":\s*"([\w.]+)"', schema.read_text()))
    return found - {'manifest', 'test', 'types', 'events', 'extensionTypes'}


def squash(name):
    return name.replace('_', '').replace('.', '').lower()


def api(path, text, known):
    """The API that the test file at `path`, with the source `text`, tests."""
    name = re.sub(r'^(test_chrome_ext_|test_ext_|browser_ext_|test_|browser_)', '',
                  Path(path).stem)
    by_name = {squash(ns): ns for ns in known}
    by_name.update({squash(alias): ns for alias, ns in ALIASES.items()})
    # The longest run of the name's first words that names an API:
    # contentscript_slow_frame is content_scripts, not contentScripts.
    words = name.split('_')
    prefixes = [squash('_'.join(words[:n])) for n in range(len(words), 0, -1)]
    found = next((by_name[prefix] for prefix in prefixes if prefix in by_name), None)
    if found:
        return found
    calls = Counter()
    for match in re.findall(r'\bbrowser\.(\w+(?:\.\w+)?)\.', text):
        namespace = match if match in known else match.split('.')[0]
        if namespace in known and namespace != 'test':
            calls[namespace] += 1
    return calls.most_common(1)[0][0] if calls else ''


def not_applicable(path):
    """The reason the test file at `path` is n/a, or None."""
    for pattern, reason in NOT_APPLICABLE:
        if re.search(pattern, path):
            return reason
    return None


def entry(path, text, known, old):
    """The inventory entry of the test file at `path`, keeping the state of
    its `old` entry when it is ported, or n/a set by hand (`by_hand`). Any
    other entry follows NOT_APPLICABLE again."""
    item = {'path': path, 'api': api(path, text, known)}
    if old and (old['state'] == 'ported' or old.get('by_hand')):
        item.update({key: old[key] for key in ('state', 'port', 'reason', 'by_hand')
                     if key in old})
        return item
    reason = not_applicable(path)
    item.update({'state': 'n/a', 'reason': reason} if reason else {'state': 'todo'})
    return item


def write(tests, source_info):
    """firefox-tests.json, one test per line."""
    lines = [json.dumps(item, ensure_ascii=False) for item in tests]
    TESTS.write_text('{\n "source": ' + json.dumps(source_info) + ',\n "tests": [\n  '
                     + ',\n  '.join(lines) + '\n ]\n}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--source', type=Path, default=SOURCE,
                        help="the clone of Firefox's source (made if missing)")
    parser.add_argument('--no-fetch', action='store_true', help='read the clone as it is')
    args = parser.parse_args()
    if not args.no_fetch:
        fetch(args.source)
    commit, date = subprocess.run(['git', '-C', str(args.source), 'log', '-1', '--format=%H %cs'],
                                  capture_output=True, text=True, check=True).stdout.split()
    old = {}
    if TESTS.is_file():
        old = {item['path']: item for item in json.loads(TESTS.read_text())['tests']}
    known = namespaces(args.source)
    paths = test_files(args.source)
    tests = [entry(path, (args.source / path).read_text(errors='replace'), known, old.get(path))
             for path in paths]
    for path in sorted(set(old) - set(paths)):
        print(f"gone from Firefox's tree: {path} ({old[path]['state']})", file=sys.stderr)
    write(tests, {'repository': REPOSITORY, 'commit': commit, 'date': date})
    states = Counter(item['state'] for item in tests)
    print(f"{TESTS}: {len(tests)} test files, "
          + ', '.join(f'{states[s]} {s}' for s in ('ported', 'todo', 'n/a')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
