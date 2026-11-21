#!/usr/bin/env python3
"""Pauses and resumes the nightly climb rounds (docs/nightly.md, "Climb rounds").

  rounds.py pause [REASON]   no new round starts until resume
  rounds.py resume           rounds start again, and one starts now
  rounds.py status

A pause is the file work/rounds-paused, with the reason in it. A round that
is running finishes its batch, and then doesn't start the next one. resume
removes the file and, when no chain is open, starts the next round as the
router does (not within 2 minutes of a chain's end, when a stage may still be
starting one itself, nor from 17:55 to 18:05, when the router starts): it appends `begin`, `round <N>` and `start climb round <N>` to
the log and posts to the climb stage's webhook. The daily 18:00 chain runs
either way; only the rounds after it wait for resume.
"""
import datetime
import re
import subprocess
import sys
import urllib.request
from pathlib import Path


def main_checkout():
    """dubna's main checkout, also when this runs from one of its worktrees."""
    common = subprocess.run(['git', '-C', str(Path(__file__).parent), 'rev-parse',
                             '--path-format=absolute', '--git-common-dir'],
                            capture_output=True, text=True, check=True).stdout.strip()
    return Path(common).parent


WORK = main_checkout() / 'work'
PAUSE = WORK / 'rounds-paused'
LOG = WORK / 'nightly.log'
WEBHOOKS = Path.home() / '.config/dubna/stage-webhooks.tsv'


def last_stamp():
    """The time of the log's last line."""
    last = LOG.read_text().splitlines()[-1]
    return datetime.datetime.fromisoformat(last.split(' ', 1)[0].replace('Z', '+00:00'))


def last_chain():
    """The log's lines from the last `begin`, without their times."""
    events = [line.split(' ', 1)[1] for line in LOG.read_text().splitlines() if ' ' in line]
    begins = [n for n, event in enumerate(events) if event == 'begin']
    return events[begins[-1]:] if begins else []


def log(event):
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    with LOG.open('a') as file:
        file.write(f'{stamp} {event}\n')


def start_round():
    chain = last_chain()
    if chain and not any(event.startswith('end chain') for event in chain):
        print(f'a chain is open ({chain[-1]}): it starts the next round when it ends')
        return 0
    now = datetime.datetime.now(datetime.timezone.utc)
    if now - last_stamp() < datetime.timedelta(minutes=2):
        sys.exit('the last chain ended under 2 minutes ago, and its stage may be starting '
                 'the next round: run resume again in a few minutes')
    local = datetime.datetime.now()
    if (local.hour, local.minute) >= (17, 55) and (local.hour, local.minute) < (18, 5):
        sys.exit('the 18:00 router starts now: run resume after 18:05')
    rounds = [int(m.group(1)) for event in chain if (m := re.fullmatch(r'round (\d+)', event))]
    number = rounds[-1] + 1 if rounds else 1
    url = next((line.split('\t')[1] for line in WEBHOOKS.read_text().splitlines()
                if line.startswith('climb\t')), None)
    if not url:
        sys.exit(f'no climb webhook in {WEBHOOKS}')
    log('begin')
    log(f'round {number}')
    log(f'start climb round {number}')
    try:
        request = urllib.request.Request(url, data=b'{}', method='POST',
                                         headers={'content-type': 'application/json'})
        with urllib.request.urlopen(request, timeout=30) as response:
            print(f'started climb round {number} (HTTP {response.status})')
    except Exception as error:  # pylint: disable=broad-except
        # The router's rule for a failed start; a 404 means the URLs were rotated.
        # Only the error's type and code: some messages carry the secret URL.
        code = getattr(error, 'code', '')
        log(f'end climb died {code}'.rstrip())
        log('end chain nothing')
        sys.exit(f'the climb webhook failed: {type(error).__name__} {code}'.rstrip())
    return 0


def main():
    command = sys.argv[1] if len(sys.argv) > 1 else 'status'
    if command == 'pause':
        PAUSE.write_text((' '.join(sys.argv[2:]) or 'paused by hand') + '\n')
        print(f'paused: {PAUSE.read_text().strip()}')
        chain = last_chain()
        if chain and not any(event.startswith('end chain') for event in chain):
            print(f'a round is running ({chain[-1]}): it finishes its batch first')
        return 0
    if command == 'resume':
        PAUSE.unlink(missing_ok=True)
        return start_round()
    if command == 'status':
        print(f'paused: {PAUSE.read_text().strip()}' if PAUSE.exists() else 'not paused')
        chain = last_chain()
        print(f'last event: {chain[-1] if chain else "none"}')
        return 0
    sys.exit(__doc__)


if __name__ == '__main__':
    sys.exit(main())
