#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Puts our translations (l10n/<lang>.json) into a prepared tree's .xtb files.

Each ROOT is a repo with our strings: dubna, and each product that has its own.
A ROOT's strings are the messages of its src/**/oganesson_*.grdp and the ones
that its patches (patches/oganesson/**, patches/*.patch) add to Chromium's
.grd/.grdp files. English is their source text; ROOT/l10n/<lang>.json has, per
message, its name, the English text it translates ("source") and the
translation ("message"), the format of Helium's i18n/translations. grit matches
a translation to its message by the English text, so a changed English text
needs a new translation.

Our own .grdp files are parts of chrome/app/oganesson_strings.grd; the messages
our patches add are in Chromium's chrome/app/generated_resources.grd. Every
translation goes into the .xtb files of both: grit takes the ones whose message
the .grd has. Applying uses the tree's own grit for the message IDs.

Usage: translate.py SRC_TREE ROOT...   apply
       translate.py --check ROOT...    each ROOT's messages have a current
                                       translation in every ROOT/l10n/<lang>.json
"""
import html, json, re, sys
import xml.etree.ElementTree as ET
from pathlib import Path

MESSAGE = re.compile(r'<message name="(\w+)"[^>]*>\s*(.*?)\s*</message>', re.S)


def our_messages(root):
    """(name, English text) of every message root adds, once each."""
    found = {}
    for grdp in sorted((root / 'src').rglob('oganesson_*.grdp')):
        for name, message in MESSAGE.findall(grdp.read_text()):
            found[(name, re.sub(r'\s+', ' ', message))] = None
    patches = [*(root / 'patches/oganesson').rglob('*.patch'), *(root / 'patches').glob('*.patch')]
    for patch in sorted(patches):
        for section in re.split(r'(?m)^(?=--- a/)', patch.read_text()):
            if not re.match(r'--- a/\S+\.grdp?\n', section):
                continue
            # The patched file's side of each hunk; a message counts as ours
            # when a line of it is added.
            for hunk in re.split(r'(?m)^@@.*\n', section)[1:]:
                lines = [l for l in hunk.splitlines() if l[:1] in ' +']
                text = '\n'.join(l[1:] for l in lines)
                added = [l[0] == '+' for l in lines]
                for m in MESSAGE.finditer(text):
                    first, last = text.count('\n', 0, m.start()), text.count('\n', 0, m.end())
                    if any(added[first:last + 1]):
                        found[(m[1], re.sub(r'\s+', ' ', m[2]))] = None
    return list(found)


def placeholders(text):
    return sorted(re.findall(r'<ph name="(\w+)"', text))


def load(lang_file):
    return {(t['name'], t['source']): t['message'] for t in json.loads(lang_file.read_text())}


def check(roots):
    errors, total = [], 0
    for root in roots:
        messages = our_messages(root)
        total += len(messages)
        for lang_file in sorted((root / 'l10n').glob('*.json')):
            translations = load(lang_file)
            where = lang_file.relative_to(root.parent)
            for name, text in messages:
                message = translations.get((name, text))
                if message is None:
                    errors.append(f'{where}: no translation of {name}: {text}')
                elif placeholders(message) != placeholders(text):
                    errors.append(f'{where}: {name} has other placeholders than its English')
            for name, text in translations.keys() - set(messages):
                errors.append(f'{where}: {name} translates a text it no longer has: {text}')
    print('\n'.join(errors) or f'translate.py: {total} messages, all translated')
    return 1 if errors else 0


PLACEHOLDER = re.compile(r'<ph name="(\w+)">.*?</ph>', re.S)


def message_id(text, tree):
    """grit's ID for a message: the fingerprint of its text with each
    placeholder shown as its name in capitals, as grit presents it."""
    sys.path.insert(0, str(tree / 'tools/grit'))
    from grit.extern import tclib  # pylint: disable=import-outside-toplevel
    shown = ''.join(html.unescape(part) if i % 2 == 0 else part.upper()
                    for i, part in enumerate(PLACEHOLDER.split(text)))
    return tclib.GenerateMessageId(shown)


def xtb_text(message):
    """A translation as an .xtb holds it: placeholders as empty <ph> tags."""
    return PLACEHOLDER.sub(lambda m: f'<ph name="{m[1]}" />', message)


def xtb_paths(grd):
    """The .xtb file of each language, from the .grd's <translations>."""
    return {f.get('lang'): grd.parent / f.get('path')
            for f in ET.parse(grd).getroot().find('translations').iter('file')}


GRDS = ('chrome/app/generated_resources.grd', 'chrome/app/oganesson_strings.grd')


def apply(tree, roots):
    for root in roots:
        for lang_file in sorted((root / 'l10n').glob('*.json')):
            entries = {message_id(text, tree): xtb_text(message)
                       for (_, text), message in load(lang_file).items()}
            lines = ''.join(f'<translation id="{i}">{t}</translation>\n' for i, t in entries.items())
            for grd in GRDS:
                xtb = xtb_paths(tree / grd)[lang_file.stem]
                content = re.sub(r'<translation id="(\d+)">.*?</translation>\n',
                                 lambda m: '' if m[1] in entries else m[0], xtb.read_text(),
                                 flags=re.S)
                xtb.write_text(content.replace('</translationbundle>',
                                               lines + '</translationbundle>', 1))
            print(f'translate.py: {root.name} {lang_file.stem}: {len(entries)} translations')

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    roots = [Path(r).resolve() for r in sys.argv[2:]]
    if sys.argv[1] == '--check':
        sys.exit(check(roots))
    apply(Path(sys.argv[1]), roots)
