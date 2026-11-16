#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""The notes of a GitHub release: the commits since the previous release, one
line each, from their subjects. Our commit subjects already say what changed
for the user, so no one writes the notes. Commits that only touch SKIP paths
(docs, CI, test and dev tooling) are left out.

Notes that touch WebExtensions end with the score of Firefox's own WebExtensions
tests (webextensions/README.md, "Test score"): how many pass, of all the test
files that aren't n/a. They touch WebExtensions when a change's subject names
WebExtensions or Firefox add-ons, or when dubna's webextensions/ changed since
the previous release (a move of the dubna submodule).

The previous release is the newest tag starting with "v" that HEAD contains.
Without one, the notes say this is the first release on GitHub, with the last
LIMIT changes.

Usage: release_notes.py REPO [--skip PATH...] [--limit N]
A PATH ending in / is a folder; "*.md" means any Markdown file.
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

# The score of the dubna tree that holds this file: the browser's submodule.
TEST_SCORE = Path(__file__).resolve().parent.parent / 'webextensions/score/oganesson-firefox-tests.json'
WEBEXTENSIONS = re.compile(r'WebExtensions|Firefox add-on', re.IGNORECASE)


def git(repo, *args):
    return subprocess.run(['git', '-C', repo, *args], capture_output=True, text=True,
                          check=True).stdout


def skipped(path, skip):
    for entry in skip:
        if entry == '*.md' and path.endswith('.md'):
            return True
        if entry.endswith('/') and path.startswith(entry):
            return True
        if path == entry:
            return True
    return False


def dubna_webextensions_changed(repo, since):
    """Whether webextensions/ in the repo's dubna submodule changed from the
    commit `since` to HEAD. False when there is no submodule, or its checkout
    lacks a commit."""
    try:
        old = git(repo, 'rev-parse', f'{since}:dubna').strip()
        new = git(repo, 'rev-parse', 'HEAD:dubna').strip()
        changed = git(str(Path(repo, 'dubna')), 'diff', '--name-only', old, new, '--',
                      'webextensions/')
    except subprocess.CalledProcessError:
        return False
    return changed.strip() != ''


def test_score(path):
    """The Firefox test score's line, or None without a score."""
    if not path.is_file():
        return None
    score = json.loads(path.read_text())
    return (f"Firefox's WebExtensions tests: {score['passing']} of {score['total']} pass "
            f"({score['ported']} ported so far), on {score['browser']}, {score['date']}.")


def notes(repo, skip, limit, score=TEST_SCORE):
    tags = [t for t in git(repo, 'tag', '--merged', 'HEAD', '--sort=-creatordate', 'v*').split()]
    span = f'{tags[0]}..HEAD' if tags else 'HEAD'
    lines = []
    # One record per commit: hash and subject, then its changed files.
    log = git(repo, 'log', '--first-parent', '--format=%x00%h %s', '--name-only', span)
    for record in log.split('\0')[1:]:
        head, *files = [line for line in record.splitlines() if line]
        if files and all(skipped(f, skip) for f in files):
            continue
        sha, subject = head.split(' ', 1)
        lines.append(f'- {subject} ({sha})')
    if not tags:
        lines = lines[:limit]
        text = ['First release on GitHub. The latest changes:', '', *lines]
    elif not lines:
        return f'A rebuild of {tags[0]}, without changes to the browser.'
    else:
        text = [f'Changes since {tags[0]}:', '', *lines]
    webextensions = any(WEBEXTENSIONS.search(line) for line in lines)
    if tags and not webextensions:
        webextensions = dubna_webextensions_changed(repo, tags[0])
    score_line = test_score(score)
    if webextensions and score_line:
        text += ['', score_line]
    return '\n'.join(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo')
    parser.add_argument('--skip', nargs='*', default=[])
    parser.add_argument('--limit', type=int, default=20)
    args = parser.parse_args()
    sys.stdout.write(notes(args.repo, args.skip, args.limit) + '\n')


if __name__ == '__main__':
    main()
