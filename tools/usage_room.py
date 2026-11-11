#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Says whether the Claude plan has room for another round of the evening task.

Usage: tools/usage_room.py

Reads the plan's usage from T3 Code's provider logs, where every Claude thread
logs a rate_limit_event with the 5-hour and weekly windows whenever they move,
so the newest event across threads is current. The first line is "room: yes",
"room: no" or "room: unknown"; each later line is one window.
There is room when the 5-hour session is under SESSION_MAX percent and the
weekly window is at least WEEKLY_MARGIN points under the share of its week that
has passed, so a round spends what would otherwise be left over at the reset.
"""
import datetime, json, sys
from pathlib import Path

LOGS = Path.home() / '.t3' / 'userdata' / 'logs' / 'provider'
SESSION_MAX = 50
WEEKLY_MARGIN = 15
WEEK = datetime.timedelta(days=7)


def newest_windows():
    """(time, unifiedWindows) of the newest rate_limit_event, or None."""
    newest = None
    logs = sorted(LOGS.glob('events.*.log*'), key=lambda p: p.stat().st_mtime)
    for log in logs[-10:]:
        for line in log.open(errors='replace'):
            if '"rate_limit_event"' not in line:
                continue
            stamp = line[1:line.index(']')]
            if newest and stamp <= newest[0]:
                continue
            payload = json.loads(line[line.index('{'):])['event']['payload']
            newest = (stamp, payload['rate_limit_info']['unifiedWindows'])
    return newest


def assess(windows, now):
    """(room, lines) for the unifiedWindows of a rate_limit_event."""
    room = True
    lines = []
    for name in ('five_hour', 'seven_day'):
        window = windows[name]
        resets = datetime.datetime.fromtimestamp(window['resetsAt'], datetime.timezone.utc)
        # A window that reset since the event is empty.
        percent = 0 if resets <= now else round(100 * window['utilization'])
        if name == 'five_hour':
            ok = percent < SESSION_MAX
            lines.append(f'{name}: {percent}% used, resets {resets:%a %H:%M} UTC')
        else:
            if resets <= now:
                resets += WEEK
            passed = 100 * (now - (resets - WEEK)) / WEEK
            ok = percent + WEEKLY_MARGIN <= passed
            lines.append(f'{name}: {percent}% used, {passed:.0f}% of the week passed, '
                         f'resets {resets:%a %H:%M} UTC')
        room = room and ok
    return room, lines


def main():
    try:
        newest = newest_windows()
    except (OSError, KeyError, ValueError) as e:
        print('room: unknown')
        print(f'T3 provider logs not readable: {e}')
        return 1
    if not newest:
        print('room: unknown')
        print(f'no rate_limit_event in {LOGS}')
        return 1
    stamp, windows = newest
    room, lines = assess(windows, datetime.datetime.now(datetime.timezone.utc))
    print('room: yes' if room else 'room: no')
    print('\n'.join(lines))
    print(f'as of {stamp}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
