#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Starts the nightly `security` stage when an upstream release is due now.

A systemd timer runs this every hour (tools/systemd/, docs/nightly.md "Security
runs"). It runs upstream_status.py and keeps the `update` and `zeroday` lines:
every Chromium release in our milestone carries security fixes. A line is new
when no `finding` line in the nightly log has it and this script hasn't sent it
before (work/security-sent). For new lines it writes work/security-pending,
which stops a running climb, and posts to the security stage's webhook. That
run waits in the nightly thread's queue, so it never overlaps a stage.
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rounds import LOG, WEBHOOKS, WORK, post, webhook

PENDING = WORK / 'security-pending'
SENT = WORK / 'security-sent'
SUPERHEAVY = WORK.parent.parent


def due():
    """upstream_status.py's `update` and `zeroday` lines."""
    env = dict(os.environ)
    if 'GITHUB_TOKEN' not in env:
        env['GITHUB_TOKEN'] = subprocess.run(['gh', 'auth', 'token'], capture_output=True,
                                             text=True, check=True).stdout.strip()
    report = subprocess.run([sys.executable, str(Path(__file__).parent / 'upstream_status.py'),
                             '--desktop', str(SUPERHEAVY / 'oganesson'),
                             '--android', str(SUPERHEAVY / 'tennessine')],
                            env=env, capture_output=True, text=True, check=True).stdout
    return [line for line in report.splitlines() if line.startswith(('update: ', 'zeroday: '))]


def new(lines, log, sent):
    """The lines that neither the log has as a `finding` nor were sent before."""
    logged = {line.split(' ', 1)[1] for line in log if ' ' in line}
    return [line for line in lines if line not in sent and f'finding {line}' not in logged]


def main():
    lines = due()
    sent = SENT.read_text().splitlines() if SENT.exists() else []
    found = new(lines, LOG.read_text().splitlines(), sent)
    if not lines:
        # A stage landed what was due: a flag left now would stop the next climb for nothing.
        PENDING.unlink(missing_ok=True)
    # Keep only the lines still due, so the file stays short.
    SENT.write_text(''.join(f'{line}\n' for line in lines if line in sent))
    if not found:
        print(f'nothing new ({len(lines)} due, already sent or logged)')
        return 0
    url = webhook('security')
    if not url:
        sys.exit(f'no security webhook in {WEBHOOKS}')
    PENDING.write_text(''.join(f'{line}\n' for line in found))
    try:
        status = post(url)
    except Exception as error:  # pylint: disable=broad-except
        # Not marked sent, so the next hour tries again.
        sys.exit(f'the security webhook failed: {type(error).__name__} '
                 f'{getattr(error, "code", "")}'.rstrip())
    SENT.write_text(''.join(f'{line}\n' for line in lines if line in sent or line in found))
    print(f'started the security stage (HTTP {status}):')
    print(''.join(f'  {line}\n' for line in found), end='')
    return 0


if __name__ == '__main__':
    sys.exit(main())
