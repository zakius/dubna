#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Projects the Claude plan's weekly usage to its reset, to say whether the
evening task can start another run.

Usage: tools/usage_pace.py

Reads the plan's usage from T3 Code's provider logs, where every Claude thread
logs a rate_limit_event with the 5-hour and weekly windows whenever they move.
Each rise of the weekly window counts for the thread whose event shows it.
The pace counts only the work outside the nightly thread: the runs this check
gates would otherwise raise the pace that refuses them. It is the faster of two
rates: the average since the week's start, or since the nightly thread's oldest
event if its rotated log no longer reaches that far, and the last 24 hours.
The 18:00 chains still to come are not projected. The first line is
"headroom: yes", "headroom: no" or "headroom: unknown"; the later lines show
the numbers behind it.
There is headroom when the 5-hour session is under SESSION_MAX percent, the
week's usage is under its even pace (100 - RESERVE points spread over the 7
days), and, at the pace, the week would have room left at its reset for another
run (RUN_COST) and still keep RESERVE points for unexpected work. The even pace
makes the runs last the whole week and wait whenever other work gets ahead of it.

Fable's own weekly limit doesn't count: when it is reached, Opus does the
reviews (docs/nightly.md, "Fable review before the push").
"""
import datetime, json, sys, time
from pathlib import Path

LOGS = Path.home() / '.t3' / 'userdata' / 'logs' / 'provider'
# The router's thread (its task's boundThreadId), where every nightly stage runs.
NIGHTLY_THREAD = 'e68f137f-f5bb-4692-a8e6-db45394ca597'
SESSION_MAX = 50
# An evening run has cost 8 to 15 points of the week. This counts it lower,
# so that more rounds start.
RUN_COST = 5
# Kept free for the maintainer's own work, whatever runs.
RESERVE = 7
WEEK = datetime.timedelta(days=7)
DAY = datetime.timedelta(days=1)
UTC = datetime.timezone.utc


def read_events():
    """(time, unifiedWindows, nightly) of every rate_limit_event of the last 8 days, oldest first."""
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
                events.append((stamp, info['unifiedWindows'], NIGHTLY_THREAD in line))
    events.sort(key=lambda event: event[0])
    return events


def when(window):
    return datetime.datetime.fromtimestamp(window['resetsAt'], UTC)


def percent(window):
    return 100 * window['utilization']


def own_usage(events, reset, since):
    """Points of the week that ends at reset, used after since outside the nightly thread."""
    own, before = 0, 0
    for stamp, windows, nightly in events:
        if when(windows['seven_day']) != reset:
            continue
        used = percent(windows['seven_day'])
        if not nightly and stamp > since:
            own += max(used - before, 0)
        # Threads running at once can report a slightly older, lower reading.
        before = max(before, used)
    return own


def assess(events, now):
    """(headroom, lines): headroom is True, False or None when unknown."""
    if not events:
        return None, ['no rate_limit_event in the T3 provider logs']
    stamp, windows, _ = events[-1]
    session, week = windows['five_hour'], windows['seven_day']
    reset = when(week)
    if reset <= now:
        return None, [f'the week reset at {reset:%a %H:%M} UTC and no thread has run since']
    start = reset - WEEK
    used = percent(week)
    session_used = 0 if when(session) <= now else percent(session)

    # The nightly thread's log rotates, so its oldest events are gone and their
    # rises would count as the maintainer's. Take the week's rate only from the
    # nightly thread's oldest event left on.
    covered = start
    for event_stamp, _, nightly in events:
        if nightly:
            covered = max(start, event_stamp)
            break
    own = own_usage(events, reset, covered)
    week_rate = own / ((now - covered) / DAY)
    recent = max(now - DAY, covered)
    day_rate = own_usage(events, reset, recent) / ((now - recent) / DAY)
    pace = max(week_rate, day_rate)
    left = 100 - (used + pace * ((reset - now) / DAY))
    even = (100 - RESERVE) * ((now - start) / WEEK)

    headroom = session_used < SESSION_MAX and used < even and left >= RUN_COST + RESERVE
    return headroom, [
        f'five_hour: {session_used:.0f}% used, resets {when(session):%a %H:%M} UTC',
        f'seven_day: {used:.0f}% used, resets {reset:%a %H:%M} UTC '
        f'({(reset - now) / datetime.timedelta(hours=1):.0f} h left)',
        f'outside the nightly thread: {own:.0f} points since {covered:%a %H:%M} UTC',
        f'pace outside the nightly thread: {week_rate:.1f} points a day since then, '
        f'{day_rate:.1f} over the last 24 h',
        f'at the reset: {max(left, 0):.0f} points left at this pace '
        f'(needs {RUN_COST + RESERVE}: {RUN_COST} for a run, {RESERVE} kept free)',
        f'even pace for the week: {even:.0f}% by now ({100 - RESERVE} at the reset)',
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
