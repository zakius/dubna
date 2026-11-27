#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Starts the nightly `security` stage when an upstream release is due now.

A systemd timer runs this every hour (tools/systemd/, docs/nightly.md "Security
runs"). It runs upstream_status.py and keeps the `update` and `zeroday` lines:
every Chromium release in our milestone carries security fixes. A line is new
when no `finding` line in the nightly log has it and this script hasn't sent it
before (work/security-sent). For new lines it posts to the security stage's
webhook. That run waits in the nightly thread's queue, so it never overlaps a
stage. work/security-pending lists the sent lines the log doesn't have yet:
while it exists, a climb stops for the security run.
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
        # gh keeps the token in the keyring, which is locked after a boot until a login.
        # Without it the report still fits GitHub's anonymous limit of 60 calls an hour.
        token = subprocess.run(['gh', 'auth', 'token'], capture_output=True, text=True)
        if token.returncode == 0 and token.stdout.strip():
            env['GITHUB_TOKEN'] = token.stdout.strip()
    report = subprocess.run([sys.executable, str(Path(__file__).parent / 'upstream_status.py'),
                             '--desktop', str(SUPERHEAVY / 'oganesson'),
                             '--android', str(SUPERHEAVY / 'tennessine')],
                            env=env, capture_output=True, text=True, check=True).stdout
    return [line for line in report.splitlines() if line.startswith(('update: ', 'zeroday: '))]


def unlogged(lines, log):
    """The lines that the log has no `finding` line for."""
    logged = {line.split(' ', 1)[1] for line in log if ' ' in line}
    return [line for line in lines if f'finding {line}' not in logged]


def write(path, lines):
    """Writes one line each, or removes the file when there are none."""
    if lines:
        path.write_text(''.join(f'{line}\n' for line in lines))
    else:
        path.unlink(missing_ok=True)


def main():
    lines = due()
    log = LOG.read_text().splitlines()
    # Only lines still due, so the file stays short.
    sent = [line for line in (SENT.read_text().splitlines() if SENT.exists() else [])
            if line in lines]
    found = [line for line in unlogged(lines, log) if line not in sent]
    if found:
        url = webhook('security')
        if not url:
            sys.exit(f'no security webhook in {WEBHOOKS}')
        # Before the post: the run can start and remove it before the post returns.
        write(PENDING, unlogged(sent, log) + found)
        try:
            status = post(url)
        except Exception as error:  # pylint: disable=broad-except
            # Not marked sent, so the next hour tries again; no climb stops for a run
            # that never comes.
            write(PENDING, unlogged(sent, log))
            sys.exit(f'the security webhook failed: {type(error).__name__} '
                     f'{getattr(error, "code", "")}'.rstrip())
        sent += found
        print(f'started the security stage (HTTP {status}):')
        print(''.join(f'  {line}\n' for line in found), end='')
    else:
        print(f'nothing new ({len(lines)} due, already sent or logged)')
    write(SENT, sent)
    if not unlogged(sent, log):
        # Every sent line is in the log, as the security stage or the 18:00 router wrote it.
        PENDING.unlink(missing_ok=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
