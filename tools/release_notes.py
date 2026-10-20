#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""The notes of a GitHub release: the commits since the previous release, one
line each, from their subjects. Our commit subjects already say what changed
for the user, so no one writes the notes. Commits that only touch SKIP paths
(docs, CI, test and dev tooling) are left out.

The previous release is the newest tag starting with "v" that HEAD contains.
Without one, the notes say this is the first release on GitHub, with the last
LIMIT changes.

Usage: release_notes.py REPO [--skip PATH...] [--limit N]
A PATH ending in / is a folder; "*.md" means any Markdown file.
"""
import argparse
import subprocess
import sys


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


def notes(repo, skip, limit):
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
        return '\n'.join(['First release on GitHub. The latest changes:', '', *lines[:limit]])
    if not lines:
        return f'A rebuild of {tags[0]}, without changes to the browser.'
    return '\n'.join([f'Changes since {tags[0]}:', '', *lines])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo')
    parser.add_argument('--skip', nargs='*', default=[])
    parser.add_argument('--limit', type=int, default=20)
    args = parser.parse_args()
    sys.stdout.write(notes(args.repo, args.skip, args.limit) + '\n')


if __name__ == '__main__':
    main()
