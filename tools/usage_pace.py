#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Projects the Claude plan's weekly usage to its reset, to say whether the
evening task can start another run.

Usage: tools/usage_pace.py

Reads the plan's usage from T3 Code's provider logs, where every Claude thread
logs a rate_limit_event with the 5-hour and weekly windows whenever they move.
The pace is the faster of two rates: the week's average so far and the last
24 hours. The first line is "headroom: yes", "headroom: no" or
"headroom: unknown"; the later lines show the numbers behind it.
There is headroom when the 5-hour session is under SESSION_MAX percent and, at
the pace, the week would have room left at its reset for another run (RUN_COST)
and still keep RESERVE points for unexpected work.
"""
import datetime, json, sys, time
from pathlib import Path

LOGS = Path.home() / '.t3' / 'userdata' / 'logs' / 'provider'
SESSION_MAX = 50
# An evening run has cost 8 to 15 points of the week.
RUN_COST = 15
# Kept free for the maintainer's own work, whatever runs.
RESERVE = 25
WEEK = datetime.timedelta(days=7)
DAY = datetime.timedelta(days=1)
UTC = datetime.timezone.utc


def read_events():
    """(time, unifiedWindows) of every rate_limit_event of the last 8 days, oldest first."""
    events = []
    cutoff = time.time() - 8 * 86400
    for log in LOGS.glob('events.*.log*'):
        if log.stat().st_mtime < cutoff:
            continue
        for line in log.open(errors='replace'):
            if '"rate_limit_event"' not in line:
                continue
            info = json.loads(line[line.index('{'):])['event']['payload']['rate_limit_info']
            if 'unifiedWindows' in info:
                stamp = datetime.datetime.fromisoformat(line[1:line.index(']')])
                events.append((stamp, info['unifiedWindows']))
    events.sort(key=lambda event: event[0])
    return events


def when(window):
    return datetime.datetime.fromtimestamp(window['resetsAt'], UTC)


def percent(window):
    return 100 * window['utilization']


def assess(events, now):
    """(headroom, lines): headroom is True, False or None when unknown."""
    if not events:
        return None, ['no rate_limit_event in the T3 provider logs']
    stamp, windows = events[-1]
    session, week = windows['five_hour'], windows['seven_day']
    reset = when(week)
    if reset <= now:
        return None, [f'the week reset at {reset:%a %H:%M} UTC and no thread has run since']
    start = reset - WEEK
    used = percent(week)
    session_used = 0 if when(session) <= now else percent(session)

    # The newest usage at least 24 hours old in this week, else the week's start.
    since, used_then = start, 0
    for event_stamp, event_windows in events:
        if event_stamp > now - DAY:
            break
        if when(event_windows['seven_day']) == reset:
            since, used_then = event_stamp, percent(event_windows['seven_day'])
    week_rate = used / ((now - start) / DAY)
    day_rate = (used - used_then) / ((now - since) / DAY)
    pace = max(week_rate, day_rate)
    left = 100 - (used + pace * ((reset - now) / DAY))

    headroom = session_used < SESSION_MAX and left >= RUN_COST + RESERVE
    return headroom, [
        f'five_hour: {session_used:.0f}% used, resets {when(session):%a %H:%M} UTC',
        f'seven_day: {used:.0f}% used, resets {reset:%a %H:%M} UTC '
        f'({(reset - now) / datetime.timedelta(hours=1):.0f} h left)',
        f'pace: {week_rate:.1f} points a day over the week, {day_rate:.1f} over the last 24 h',
        f'at the reset: {max(left, 0):.0f} points left at this pace '
        f'(needs {RUN_COST + RESERVE}: {RUN_COST} for a run, {RESERVE} kept free)',
        f'as of {stamp:%Y-%m-%d %H:%M} UTC',
    ]


def main():
    try:
        events = read_events()
    except (OSError, KeyError, ValueError) as e:
        print('headroom: unknown')
        print(f'T3 provider logs not readable: {e}')
        return 1
    headroom, lines = assess(events, datetime.datetime.now(UTC))
    print({True: 'headroom: yes', False: 'headroom: no', None: 'headroom: unknown'}[headroom])
    print('\n'.join(lines))
    return 0 if headroom is not None else 1


if __name__ == '__main__':
    sys.exit(main())
