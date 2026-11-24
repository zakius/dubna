#!/usr/bin/env python3
"""Checks that what firefox-add-ons.patch adds to Chromium's extension APIs is Firefox add-ons'
alone (webextensions/README.md, "Firefox add-ons only"). A Chrome Web Store extension runs on
the same code, and may tell Chrome from Firefox by any difference it sees.

  firefox_gates.py                 check; exit 1 with each problem
  firefox_gates.py --everyone      print the entries the failing hunks need (check 4)
  firefox_gates.py --refresh TREE  copy Chromium's files from TREE, a Chromium tree with the
                                   series before firefox-add-ons.patch applied

The checks need no Chromium tree: webextensions/upstream/ holds Chromium's version of each JSON
file the patch changes, as the series before the patch leaves it.

1. API schemas. The patch's hunks of each schema, applied to Chromium's version, give our
   schema. Its Chromium view (src/extensions/renderer/bindings/firefox_schema_view.h: without
   the nodes marked "firefox_only", with each "chromium" override applied) must be Chromium's
   schema, apart from what the bindings never read (descriptions and the like). Every other
   extension's bindings read that view.
2. API features. A feature the patch adds or changes must take Chromium's delegated
   availability check ("requires_delegated_availability_check": true) in each entry it adds or
   changes, and be in src/extensions/common/firefox_add_on_availability.cc's list, which must
   name no other feature. A namespace of a schema in src/ is such a feature too. An internal
   feature, which only custom bindings reach, needs neither.
3. Permission and manifest features, which can't take the check: each change is in
   ALLOWED_CHANGES below, with what gates it in C++.
4. Code. Each hunk of the patch that changes code in a Chromium file (C++, JavaScript) must
   name the gate in its changed lines (GATE), call a function of src/ whose file names it, or
   be in webextensions/everyone.json, which says why every extension gets it. An entry there is
   the hunk's file and the hash of its changed lines, which the failure prints; an entry that no
   hunk matches any more fails too, so the list stays exact. `--everyone` prints the entries the
   hunks that fail need, for a reviewer to give each its reason. A hunk that names the gate
   passes as a whole, so keep a change for every extension out of a Firefox add-on's hunk.

A failing check names the file and the node. When Chromium moves to a new milestone and the
patch is regenerated, the hunks no longer apply to the old copies: refresh them.
"""
import copy
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATCH = ROOT / 'patches/firefox-add-ons.patch'
UPSTREAM = ROOT / 'webextensions/upstream'
SRC = ROOT / 'src'
AVAILABILITY = SRC / 'extensions/common/firefox_add_on_availability.cc'
EVERYONE = ROOT / 'webextensions/everyone.json'

# What decides that a Firefox add-on gets the change and other extensions don't.
GATE = re.compile(r'IsFirefoxAddOn|IsFirefoxContext|IsHiddenFromFirefoxAddOn|firefox_add_on\b|'
                  r'firefox_add_on_\b|HideFirefoxOnlyMembers|ForContext\(|chromium_view_')
# Files whose hunks change no behaviour of their own: build files, strings, tests, and
# declarations (headers, mojom), whose code runs where a source file calls it.
NOT_CODE = re.compile(r'BUILD\.gn$|\.gni$|\.grdp?$|_unittest\.cc$|_browsertest\.cc$|\.json$|'
                      r'\.h$|\.mojom$')
CALL = re.compile(r'\b([A-Za-z]\w*)\(')

# Keys that an entry may add to Chromium's own and still give no extension more.
NARROWING = ('min_manifest_version', 'max_manifest_version', 'default_parent')
FEATURE_FILES = ('_api_features.json', '_permission_features.json', '_manifest_features.json',
                 '_behavior_features.json')
# The keys that the schema bundle leaves out (json_schema_compiler/cpp_bundle_generator.py,
# _RemoveUnneededFields): the bindings never see them.
UNREAD_KEYS = ('description', 'compiler_options', 'nodoc', 'nocompile', 'manifest_keys')

# Feature changes that the delegated check doesn't gate, with what gates each one instead. file: {feature: reason}.
ALLOWED_CHANGES = {
    'chrome/common/extensions/api/_api_features.json': {
        'contextMenus': 'its alias, menus, is a feature of its own, which is Firefox add-ons\'',
    },
    'chrome/common/extensions/api/_permission_features.json': {
        'browserSettings': 'the browserSettings namespace is Firefox add-ons\' alone',
        'theme': 'the theme namespace is Firefox add-ons\' alone',
        'contextualIdentities': 'the contextualIdentities namespace is Firefox add-ons\' alone',
        'scripting': 'the scripting namespace in Manifest V2 is Firefox add-ons\' alone',
    },
    'extensions/common/api/_manifest_features.json': {
        'background.scripts': 'BackgroundInfo::Parse() drops a Manifest V3 background page '
                              'unless IsFirefoxAddOn()',
        'background.page': 'BackgroundInfo::Parse() drops a Manifest V3 background page '
                           'unless IsFirefoxAddOn()',
        'page_action': 'HasChromiumPageAction() leaves out a Manifest V3 page_action unless '
                       'IsFirefoxAddOn(), and the pageAction API feature checks it',
    },
}


def strip_comments(text):
    """JSON without the // and /* */ comments Chromium's JSON files have
    (tools/json_comment_eater)."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith('//', i):
            i = text.find('\n', i)
            i = n if i < 0 else i
        elif text.startswith('/*', i):
            i = text.index('*/', i) + 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def parse_json(text):
    return json.loads(strip_comments(text))


def sections(patch_text):
    """{path: [hunk, ...]} of a patch; a hunk is (old_start, [lines])."""
    result, path, hunk = {}, None, None
    lines = patch_text.splitlines()
    for i, line in enumerate(lines):
        if line.startswith('--- ') and i + 1 < len(lines) and lines[i + 1].startswith('+++ '):
            hunk = None
        elif line.startswith('+++ ') and hunk is None:
            path = re.sub(r'^b/', '', line[4:].split('\t')[0])
            result[path] = []
        elif path and line.startswith('@@'):
            hunk = (int(re.match(r'@@ -(\d+)', line).group(1)), [])
            result[path].append(hunk)
        elif hunk is not None and line[:1] in (' ', '+', '-', '\\'):
            hunk[1].append(line)
    return result


def apply(text, hunks, path):
    """`text` with `hunks` applied exactly, or a ValueError."""
    lines = text.split('\n')
    out, pos = [], 0
    for start, body in hunks:
        old = [l[1:] for l in body if l[:1] in (' ', '-')]
        new = [l[1:] for l in body if l[:1] in (' ', '+')]
        at = start - 1 if old else start
        if lines[at:at + len(old)] != old:
            raise ValueError(f'{path}: a hunk at line {start} no longer applies to '
                             'webextensions/upstream/; run --refresh')
        out += lines[pos:at] + new
        pos = at + len(old)
    return '\n'.join(out + lines[pos:])


def is_firefox_only(node):
    return isinstance(node, dict) and node.get('firefox_only') is True


def chromium_view(node):
    """firefox_schema_view.cc's ChromiumView()."""
    if isinstance(node, list):
        return [chromium_view(item) for item in node if not is_firefox_only(item)]
    if not isinstance(node, dict):
        return copy.deepcopy(node)
    view = {key: chromium_view(value) for key, value in node.items()
            if key not in ('chromium', 'firefox_only') and not is_firefox_only(value)}
    for key, value in node.get('chromium', {}).items():
        if value is None:
            view.pop(key, None)
        else:
            view[key] = copy.deepcopy(value)
    return view


def unread_removed(node):
    if isinstance(node, list):
        return [unread_removed(item) for item in node]
    if isinstance(node, dict):
        return {key: unread_removed(value) for key, value in node.items()
                if key not in UNREAD_KEYS}
    return node


def first_difference(a, b, where=''):
    """Where `a` and `b` first differ, or None."""
    if isinstance(a, dict) and isinstance(b, dict):
        for key in sorted(set(a) | set(b)):
            if key not in a or key not in b:
                return f'{where}.{key}: ' + ('only ours' if key in a else 'only Chromium\'s')
            found = first_difference(a[key], b[key], f'{where}.{key}')
            if found:
                return found
        return None
    if isinstance(a, list) and isinstance(b, list):
        for i, (x, y) in enumerate(zip(a, b)):
            label = x.get('name', x.get('id', i)) if isinstance(x, dict) else i
            found = first_difference(x, y, f'{where}[{label}]')
            if found:
                return found
        if len(a) != len(b):
            return f'{where}: {len(a)} items, Chromium has {len(b)}'
        return None
    return None if a == b else f'{where}: {json.dumps(a)} where Chromium has {json.dumps(b)}'


def gated_features():
    return set(re.findall(r'^\s+"([\w.]+)",$', AVAILABILITY.read_text(), re.M))


def entries(value):
    return value if isinstance(value, list) else [value]


def check_api_features(path, before, after, problems, internal):
    flagged = set()
    for name, value in after.items():
        if not all(isinstance(e, dict) for e in entries(value)):
            continue
        # An internal feature is only custom bindings', never on chrome.*.
        if all(e.get('internal') for e in entries(value)):
            internal.add(name)
            continue
        if before.get(name) == value:
            continue
        if any(e.get('requires_delegated_availability_check') for e in entries(value)):
            flagged.add(name)
        if name in ALLOWED_CHANGES.get(path, {}):
            continue
        old = entries(before.get(name, []))
        for entry in entries(value):
            narrowed = {k: v for k, v in entry.items() if k not in NARROWING}
            if (entry not in old and narrowed not in old and
                    not entry.get('requires_delegated_availability_check')):
                problems.append(f'{path}: "{name}" changes for every extension; give the entry '
                                'that changes "requires_delegated_availability_check": true')
    return flagged


def check(patch_text=None):
    """The problems found, as messages."""
    patch_text = PATCH.read_text() if patch_text is None else patch_text
    problems, flagged, internal = [], set(), set()
    for path, hunks in sections(patch_text).items():
        if not path.endswith('.json'):
            continue
        upstream = UPSTREAM / path
        if not upstream.exists():
            problems.append(f'{path}: no copy in webextensions/upstream/; run --refresh')
            continue
        try:
            before = parse_json(upstream.read_text())
            after = parse_json(apply(upstream.read_text(), hunks, path))
        except ValueError as error:
            problems.append(str(error))
            continue
        name = path.rsplit('/', 1)[1]
        if name == '_api_features.json':
            flagged |= check_api_features(path, before, after, problems, internal)
        elif name in FEATURE_FILES:
            allowed = ALLOWED_CHANGES.get(path, {})
            for key in sorted(set(before) | set(after)):
                if before.get(key) != after.get(key) and key not in allowed:
                    problems.append(f'{path}: "{key}" changes for every extension; gate it in '
                                    'C++ and add it to ALLOWED_CHANGES with how')
        else:
            difference = first_difference(unread_removed(chromium_view(after)),
                                          unread_removed(before))
            if difference:
                problems.append(f'{path}: the Chromium view differs from Chromium\'s schema at '
                                f'{difference}; mark it "firefox_only" or give it "chromium"')
    for schema in sorted(SRC.rglob('*/api/*.json')):
        if schema.name.startswith('_'):
            continue
        for namespace in parse_json(schema.read_text()):
            if namespace['namespace'] not in flagged | internal:
                problems.append(f'src/{schema.relative_to(SRC)}: the namespace '
                                f'"{namespace["namespace"]}" is every extension\'s; flag its '
                                'feature with "requires_delegated_availability_check": true')
    check_code(patch_text, problems)
    listed = gated_features()
    for name in sorted(flagged ^ listed):
        if name in listed:
            problems.append(f'{AVAILABILITY.name} lists "{name}", which has no feature with the '
                            'flag: no extension gets it')
        else:
            problems.append(f'"{name}" has the flag but isn\'t in {AVAILABILITY.name}: no '
                            'extension gets it')
    return problems


def gated_helpers():
    """The names of the functions that src/'s firefox_ files declare or define, in a file (or
    header and source pair) that names the gate."""
    names = set()
    for header in SRC.rglob('firefox_*.h'):
        pair = header.read_text() + (header.with_suffix('.cc').read_text()
                                     if header.with_suffix('.cc').exists() else '')
        if GATE.search(pair):
            names |= set(re.findall(r'\b([A-Z]\w+)\(', header.read_text()))
    for source in SRC.rglob('firefox_*.cc'):
        # Methods of Chromium's classes that src/ defines, such as ContextMenuMatcher's.
        if GATE.search(source.read_text()):
            names |= set(re.findall(r'^[\w:<>*& ]*?\b\w+::([A-Z]\w+)\(', source.read_text(),
                                    re.M))
    for script in SRC.rglob('firefox_*.js'):
        if GATE.search(script.read_text()):
            names |= set(re.findall(r'^function (\w+)\(', script.read_text(), re.M))
    return names


def changed_code(body):
    """A hunk's changed lines that are code: not comments, includes or blank."""
    code = []
    for line in body:
        if line[:1] not in ('+', '-'):
            continue
        text = line[1:].strip()
        if text and not text.startswith(('//', '#include', '/*', '*', '#')):
            code.append(line[0] + text)
    return code


def hunk_key(path, code):
    return f'{path}#' + hashlib.sha1('\n'.join(code).encode()).hexdigest()[:12]


def check_code(patch_text, problems, report=None):
    everyone = json.loads(EVERYONE.read_text()) if EVERYONE.exists() else {}
    helpers = gated_helpers()
    seen = set()
    for path, hunks in sections(patch_text).items():
        if NOT_CODE.search(path):
            continue
        for start, body in hunks:
            code = changed_code(body)
            if not code:
                continue
            text = '\n'.join(code)
            if GATE.search(text) or set(CALL.findall(text)) & helpers:
                continue
            key = hunk_key(path, code)
            seen.add(key)
            if key in everyone:
                continue
            if report is not None:
                report[key] = 'TODO: why every extension gets this'
            problems.append(f'{path}: the hunk at line {start} applies to every extension: gate '
                            f'it on IsFirefoxAddOn(), or add "{key}" to everyone.json with why')
    for key in sorted(set(everyone) - seen):
        problems.append(f'everyone.json: no hunk matches "{key}" any more; remove it')


def refresh(tree):
    shutil.rmtree(UPSTREAM, ignore_errors=True)
    for path in sections(PATCH.read_text()):
        if path.endswith('.json') and (tree / path).exists():
            (UPSTREAM / path).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(tree / path, UPSTREAM / path)
            print('copied', path)


def main():
    if sys.argv[1:2] == ['--refresh'] and len(sys.argv) == 3:
        refresh(Path(sys.argv[2]))
        return 0
    if sys.argv[1:] == ['--everyone']:
        report = {}
        check_code(PATCH.read_text(), [], report)
        print(json.dumps(report, indent=2))
        return 0
    if len(sys.argv) > 1:
        sys.exit(__doc__)
    problems = check()
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
