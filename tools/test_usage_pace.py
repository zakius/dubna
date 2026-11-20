#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks usage_pace.assess on made-up usage; no T3 logs."""
import datetime, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from usage_pace import assess

UTC = datetime.timezone.utc
START = datetime.datetime(2026, 11, 6, 2, tzinfo=UTC)
RESET = START + datetime.timedelta(days=7)


def at(day, hour=0):
    return START + datetime.timedelta(days=day, hours=hour)


def event(time, weekly, session=2, nightly=False):
    windows = {'five_hour': {'utilization': session / 100,
                             'resetsAt': (time + datetime.timedelta(hours=3)).timestamp()},
               'seven_day': {'utilization': weekly / 100, 'resetsAt': RESET.timestamp()}}
    return time, windows, nightly


def headroom(events, now):
    return assess(events, now)[0]


# 6 points a day for 5 days: 42 at the reset, 58 left.
steady = [event(at(day), 6 * day) for day in range(1, 6)]
assert headroom(steady, at(5))
# 13 points a day leaves 9: room for a run, but not for the reserve too.
assert not headroom([event(at(day), 13 * day) for day in range(1, 6)], at(5))
# The steady week, but the last day took 25: at that pace nothing is left.
burst = steady[:-1] + [event(at(5), 49)]
assert not headroom(burst, at(5))
# A busy 5-hour session stops a run whatever the week says.
assert not headroom(steady[:-1] + [event(at(5), 30, session=60)], at(5))
# Once that session has reset, it no longer counts.
assert headroom(steady[:-1] + [event(at(5), 30, session=60)], at(5, 4))
# 80 used with a day and a half left, as on 2026-11-11: no headroom.
assert not headroom([event(at(4, 6), 66), event(at(5, 6), 80)], at(5, 6))
# A quiet week near its reset: little used, so a run fits.
assert headroom([event(at(5), 30), event(at(6, 12), 32)], at(6, 12))
# 26 used in the week's first two days, about as on 2026-11-14: at that pace no run
# fits, but when the nightly runs used 14 of them, the maintainer's own pace leaves room.
assert not headroom([event(at(1), 13), event(at(2, 1), 26)], at(2, 1))
assert headroom([event(at(1), 6), event(at(1, 6), 14, nightly=True),
                 event(at(2), 20), event(at(2, 1), 26, nightly=True)], at(2, 1))
# Before the nightly thread's oldest event left, every rise is of unknown origin:
# the week's rate starts there. Here the 48 points of day 1 count for nothing,
# and the 3 own points of day 3 make 1.5 a day.
assert headroom([event(at(1), 48), event(at(2), 49, nightly=True),
                 event(at(3), 52), event(at(3, 1), 52, nightly=True)], at(4))
# The same holds for the last 24 hours when the nightly log covers less: the
# 50 points before its oldest event are not the maintainer's 50 points a day.
assert headroom([event(at(3, 6), 50), event(at(3, 18), 52, nightly=True),
                 event(at(3, 20), 53)], at(4, 2))
# The nightly runs keep to the week's even pace, 93 points over 7 days: 5 used
# in the week's first 5 hours, as on 2026-11-20, is ahead of it, and a day later
# it is behind. Their own usage counts here, though the projection leaves it out.
assert not headroom([event(at(0, 4), 5, nightly=True)], at(0, 5))
assert headroom([event(at(0, 4), 5, nightly=True)], at(1))
assert not headroom([event(at(2), 30, nightly=True)], at(2, 1))
# A thread's older, lower reading (13 after 14) doesn't make the next 14 a rise.
lines = assess([event(at(1), 10), event(at(1, 1), 14, nightly=True),
                event(at(1, 2), 13), event(at(1, 3), 14)], at(2))[1]
assert 'outside the nightly thread: 0 points' in lines[2], lines
# No events, or none since the weekly reset: unknown.
assert headroom([], at(1)) is None
assert headroom(steady, at(7, 1)) is None

print('ok')
