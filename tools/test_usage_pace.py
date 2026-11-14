#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks usage_pace.assess on made-up usage; no T3 logs."""
import datetime, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from usage_pace import assess, assess_fable

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
# 28 used in the week's first two days, as on 2026-11-14: at that pace no run fits,
# but when the nightly runs used 16 of them, the maintainer's own pace leaves room.
assert not headroom([event(at(1), 14), event(at(2), 28)], at(2))
assert headroom([event(at(1), 6), event(at(1, 6), 14, nightly=True),
                 event(at(2), 20), event(at(2, 1), 28, nightly=True)], at(2, 1))
# Before the nightly thread's oldest event left, every rise is of unknown origin:
# the week's rate starts there. Here the 50 points of day 1 count for nothing,
# and the 8 own points of day 3 make 4 a day.
assert headroom([event(at(1), 50), event(at(2), 55, nightly=True),
                 event(at(3), 63), event(at(3, 1), 65, nightly=True)], at(4))
# No events, or none since the weekly reset: unknown.
assert headroom([], at(1)) is None
assert headroom(steady, at(7, 1)) is None

# Fable: 85 used leaves 15, enough for a run's reviews; 90 used doesn't.
def fable(percent):
    return {'percent': percent, 'resets_at': RESET.isoformat()}
assert assess_fable(fable(85))[0]
assert not assess_fable(fable(90))[0]
print('ok')
