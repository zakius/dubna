#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Says whether the Claude plan has room for another round of the evening task.

Usage: tools/usage_room.py

Reads the plan's usage from the endpoint Claude Code's /usage reads, with
Claude Code's own sign-in (~/.claude/.credentials.json). The first line is
"room: yes", "room: no" or "room: unknown"; each later line is one limit.
There is room when the 5-hour session is under SESSION_MAX percent and every
weekly limit is at least WEEKLY_MARGIN points under the share of its week that
has passed, so a round spends what would otherwise be left over at the reset.
"""
import datetime, json, subprocess, sys, time, urllib.error, urllib.request
from pathlib import Path

URL = 'https://api.anthropic.com/api/oauth/usage'
CREDENTIALS = Path.home() / '.claude' / '.credentials.json'
SESSION_MAX = 50
WEEKLY_MARGIN = 15
WEEK = datetime.timedelta(days=7)


def fetch():
    token = json.loads(CREDENTIALS.read_text())['claudeAiOauth']['accessToken']
    # The endpoint answers 429 to every client but Claude Code.
    version = subprocess.run(['claude', '--version'], capture_output=True, text=True,
                             check=True).stdout.split()[0]
    req = urllib.request.Request(URL, headers={
        'Authorization': f'Bearer {token}',
        'anthropic-beta': 'oauth-2025-04-20',
        'User-Agent': f'claude-code/{version}'})
    # Calls close together can still get a 429; one retry is enough.
    for attempt in range(2):
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                return json.load(response)
        except urllib.error.HTTPError as e:
            if e.code != 429 or attempt:
                raise
            time.sleep(60)


def assess(usage, now):
    """(room, lines): room is True or False, lines describe each limit."""
    room = True
    lines = []
    for limit in usage['limits']:
        name = limit['kind']
        if limit.get('scope'):
            name += ' ' + limit['scope']['model']['display_name']
        percent = limit['percent']
        resets = datetime.datetime.fromisoformat(limit['resets_at'])
        if limit['group'] == 'session':
            ok = percent < SESSION_MAX
            lines.append(f'{name}: {percent}% used, resets {resets:%a %H:%M} UTC')
        else:
            passed = 100 * (now - (resets - WEEK)) / WEEK
            ok = percent + WEEKLY_MARGIN <= passed
            lines.append(f'{name}: {percent}% used, {passed:.0f}% of the week passed, '
                         f'resets {resets:%a %H:%M} UTC')
        room = room and ok
    return room, lines


def main():
    try:
        usage = fetch()
    except (OSError, KeyError, ValueError, subprocess.CalledProcessError) as e:
        print('room: unknown')
        print(f'usage not readable: {e}')
        return 1
    room, lines = assess(usage, datetime.datetime.now(datetime.timezone.utc))
    print('room: yes' if room else 'room: no')
    print('\n'.join(lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
