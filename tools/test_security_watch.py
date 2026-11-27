#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Checks security_watch.unlogged, which decides what is sent; no network."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from security_watch import unlogged

UPDATE = 'update: oganesson: helium-linux 0.22.6.1 has Chromium 158.0.8127.99, we build 158.0.8127.88'
ZERODAY = 'zeroday: tennessine: CVE-2026-1234 exploited in the wild, fixed in Chromium 158.0.8127.99'

# Nothing logged: both are new.
assert unlogged([UPDATE, ZERODAY], []) == [UPDATE, ZERODAY]
# The router logged one at its 18:00 begin: only the other.
log = ['2026-11-27T17:00:12Z begin', f'2026-11-27T17:00:40Z finding {UPDATE}']
assert unlogged([UPDATE, ZERODAY], log) == [ZERODAY]
# A line the log has only as a stage's details is still unlogged.
assert unlogged([UPDATE], [f'2026-11-27T17:00:50Z start update-oganesson {UPDATE}']) == [UPDATE]
print('ok')
