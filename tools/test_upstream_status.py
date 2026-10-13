#!/usr/bin/env python3
"""Checks upstream_status.assess on made-up releases; no network."""
import datetime, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from upstream_status import assess

D = datetime.date
TODAY = D(2026, 10, 20)


def kinds(releases, stable=(), current=None, today=TODAY, zerodays=()):
    return sorted(k for k, _ in assess(155, releases, list(stable), current or {}, today,
                                       zerodays))


desktop_155 = [((155, 0, 8059, 39), D(2026, 10, 8), '0.19.2.1')]
android_155 = [((155, 0, 8059, 39), D(2026, 10, 8), '155.0.8059.39.0')]
android_156 = [((156, 0, 8078, 25), D(2026, 10, 18), '156.0.8078.25.0')] + android_155
v39 = (155, 0, 8059, 39)

# Both on the milestone, stable not ahead: nothing due.
assert kinds({'oganesson': desktop_155, 'tennessine': android_155}, [(v39, D(2026, 10, 6))]) == []
# One upstream on 156: wait for the other, and the leader stays on 155.
assert kinds({'oganesson': desktop_155, 'tennessine': android_156},
             current={'oganesson': v39, 'tennessine': v39}) == ['waiting']
# Both on 156: move.
desktop_156 = [((156, 0, 8078, 25), D(2026, 10, 19), '0.20.0.1')] + desktop_155
assert kinds({'oganesson': desktop_156, 'tennessine': android_156},
             current={'oganesson': v39, 'tennessine': v39}) == ['milestone', 'update', 'update']
# A newer release in the milestone is taken without waiting for the other upstream.
android_41 = [((155, 0, 8059, 41), D(2026, 10, 15), '155.0.8059.41.0')] + android_155
assert kinds({'oganesson': desktop_155, 'tennessine': android_41},
             current={'oganesson': v39, 'tennessine': v39}) == ['update']
# Stable ahead of what the upstreams offer: security only after the grace period.
stable = [((155, 0, 8059, 50), D(2026, 10, 10)), (v39, D(2026, 10, 6))]
both = {'oganesson': desktop_155, 'tennessine': android_155}
assert kinds(both, stable, today=D(2026, 10, 16)) == []
assert kinds(both, stable, today=D(2026, 10, 18)) == ['security', 'security']
# An exploit in the wild: no grace period, and the milestone rule doesn't hold
# it back. A product already on the fix is left alone.
fixed_in_156 = [((156, 0, 8078, 25), D(2026, 10, 19), ['CVE-2026-1'])]
assert kinds({'oganesson': desktop_155, 'tennessine': android_156}, current={'oganesson': v39,
             'tennessine': (156, 0, 8078, 25)}, zerodays=fixed_in_156) == ['waiting', 'zeroday']
fixed_in_39 = [(v39, D(2026, 10, 8), ['CVE-2026-2'])]
assert kinds(both, current={'oganesson': v39, 'tennessine': v39}, zerodays=fixed_in_39) == []
print('ok')
