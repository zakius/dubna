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


def event(time, weekly, session=2):
    windows = {'five_hour': {'utilization': session / 100,
                             'resetsAt': (time + datetime.timedelta(hours=3)).timestamp()},
               'seven_day': {'utilization': weekly / 100, 'resetsAt': RESET.timestamp()}}
    return time, windows


def headroom(events, now):
    return assess(events, now)[0]


# 10 points a day for 5 days: 70 at the reset, 30 left.
steady = [event(at(day), 10 * day) for day in range(1, 6)]
assert headroom(steady, at(5))
# The same week, but the last day took 30: at that pace nothing is left.
burst = steady[:-1] + [event(at(5), 70)]
assert not headroom(burst, at(5))
# A busy 5-hour session stops a run whatever the week says.
assert not headroom(steady[:-1] + [event(at(5), 50, session=60)], at(5))
# Once that session has reset, it no longer counts.
assert headroom(steady[:-1] + [event(at(5), 50, session=60)], at(5, 4))
# 80 used with a day and a half left, as on 2026-11-11: no headroom.
assert not headroom([event(at(4, 6), 66), event(at(5, 6), 80)], at(5, 6))
# A quiet week near its reset: little used, so a run fits.
assert headroom([event(at(5), 30), event(at(6, 12), 32)], at(6, 12))
# No events, or none since the weekly reset: unknown.
assert headroom([], at(1)) is None
assert headroom(steady, at(7, 1)) is None
print('ok')
