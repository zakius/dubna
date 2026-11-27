#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks which due lines security_watch.new sends; no network."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from security_watch import new

UPDATE = 'update: oganesson: helium-linux 0.22.6.1 has Chromium 158.0.8127.99, we build 158.0.8127.88'
ZERODAY = 'zeroday: tennessine: CVE-2026-1234 exploited in the wild, fixed in Chromium 158.0.8127.99'

# Nothing logged or sent: both go.
assert new([UPDATE, ZERODAY], [], []) == [UPDATE, ZERODAY]
# The router logged one at its 18:00 begin: only the other goes.
log = ['2026-11-27T17:00:12Z begin', f'2026-11-27T17:00:40Z finding {UPDATE}']
assert new([UPDATE, ZERODAY], log, []) == [ZERODAY]
# Sent an hour ago: not again, even before the security stage logged it.
assert new([UPDATE, ZERODAY], [], [ZERODAY]) == [UPDATE]
# A line the log has only as a stage's details is still new.
assert new([UPDATE], [f'2026-11-27T17:00:50Z start update-oganesson {UPDATE}'], []) == [UPDATE]
print('ok')
