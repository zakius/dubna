#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks usage_room.assess on made-up usage; no network."""
import datetime, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from usage_room import assess

RESET = '2026-11-13T02:00:00+00:00'
UTC = datetime.timezone.utc


def room(session, weekly, now):
    usage = {'limits': [
        {'kind': 'session', 'group': 'session', 'percent': session,
         'resets_at': '2026-11-11T11:20:00+00:00', 'scope': None},
        {'kind': 'weekly_all', 'group': 'weekly', 'percent': weekly,
         'resets_at': RESET, 'scope': None}]}
    return assess(usage, now)[0]


# The week started 2026-11-06 02:00; on 11-11 08:26 about 75% of it has passed.
LATE = datetime.datetime(2026, 11, 11, 8, 26, tzinfo=UTC)
assert room(2, 55, LATE)
assert not room(2, 65, LATE)
# A busy 5-hour session stops a round whatever the week says.
assert not room(60, 10, LATE)
# Early in the week the same usage is ahead of pace.
assert not room(2, 20, datetime.datetime(2026, 11, 7, 2, tzinfo=UTC))
print('ok')
