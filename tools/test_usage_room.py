#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks usage_room.assess on made-up usage; no T3 logs."""
import datetime, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from usage_room import assess

UTC = datetime.timezone.utc
SESSION_RESET = datetime.datetime(2026, 11, 11, 11, 20, tzinfo=UTC).timestamp()
WEEK_RESET = datetime.datetime(2026, 11, 13, 2, tzinfo=UTC).timestamp()


def room(session, weekly, now):
    windows = {'five_hour': {'utilization': session / 100, 'resetsAt': SESSION_RESET},
               'seven_day': {'utilization': weekly / 100, 'resetsAt': WEEK_RESET}}
    return assess(windows, now)[0]


# The week started 2026-11-06 02:00; on 11-11 08:26 about 75% of it has passed.
LATE = datetime.datetime(2026, 11, 11, 8, 26, tzinfo=UTC)
assert room(2, 55, LATE)
assert not room(2, 65, LATE)
# A busy 5-hour session stops a round whatever the week says.
assert not room(60, 10, LATE)
# Once the session window has reset, its old usage no longer counts.
assert room(60, 10, datetime.datetime(2026, 11, 11, 12, tzinfo=UTC))
# Early in the week the same usage is ahead of pace.
assert not room(2, 20, datetime.datetime(2026, 11, 7, 2, tzinfo=UTC))
# After the weekly reset, an old event's usage is gone and a new week starts.
assert room(2, 90, datetime.datetime(2026, 11, 14, 22, tzinfo=UTC))
print('ok')
