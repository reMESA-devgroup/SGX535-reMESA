#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read-only, bounded SSH boot monitor. Never changes boot or GPU controls."""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import shlex
import subprocess
import time
import uuid


REMOTE = """from pathlib import Path as P
import json, os
print(json.dumps(dict(boot=P('/proc/sys/kernel/random/boot_id').read_text().strip(),
 kernel=os.uname().release, uptime_seconds=float(P('/proc/uptime').read_text().split()[0]),
 controls={p.name:p.read_text().strip() for p in
 P('/sys/module/gma500_gfx/parameters').glob('remesa*')},
 source_present=P('/proc/sgx535_source_guard').exists())))
"""


def probe(argv, timeout):
    start = time.monotonic()
    try:
        result = subprocess.run(argv + ['python3 -c ' + shlex.quote(REMOTE)],
                                stdin=subprocess.DEVNULL, capture_output=True,
                                timeout=timeout)
        # Do not log argv, stdout or stderr: transport wrappers may contain secrets.
        if result.returncode:
            row = dict(state='SSH_UNAVAILABLE', exit=result.returncode)
        else:
            try:
                value = json.loads(result.stdout)
                if (not isinstance(value, dict) or not isinstance(value.get('boot'), str)
                        or not isinstance(value.get('kernel'), str)
                        or not isinstance(value.get('controls'), dict)
                        or not isinstance(value.get('source_present'), bool)):
                    raise ValueError('invalid observation')
                boot = str(uuid.UUID(value['boot']))
                kernel = value['kernel']
                uptime = value.get('uptime_seconds')
                controls = value['controls']
                if (not re.fullmatch(r'[A-Za-z0-9._+~-]{1,128}', kernel)
                        or type(uptime) not in (int, float)
                        or not math.isfinite(uptime) or uptime < 0
                        or any(not isinstance(k, str)
                               or not re.fullmatch(r'remesa[A-Za-z0-9_]{0,96}', k)
                               or not isinstance(v, str) or v not in ('Y', 'N', '0', '1')
                               for k, v in controls.items())):
                    raise ValueError('invalid observation')
                # Emit only the explicitly requested, validated fields.
                row = dict(state='SSH_ONLINE', observation=dict(
                    boot=boot, kernel=kernel, uptime_seconds=uptime,
                    controls=controls, source_present=value['source_present']))
            except (ValueError, TypeError):
                row = dict(state='SSH_ONLINE_INVALID_OBSERVATION')
    except subprocess.TimeoutExpired:
        row = dict(state='SSH_TIMEOUT')
    except OSError:
        row = dict(state='LOCAL_TRANSPORT_ERROR')
    row.update(time_utc=datetime.now(timezone.utc).isoformat(),
               elapsed_seconds=round(time.monotonic()-start, 3))
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh-argv-file', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New JSONL log; refuses overwrite')
    parser.add_argument('--interval', type=float, default=10)
    parser.add_argument('--timeout', type=float, default=8)
    parser.add_argument('--duration', type=float, default=600)
    parser.add_argument('--stop-on-online', action='store_true')
    args = parser.parse_args()
    if not (1 <= args.interval <= 60 and 1 <= args.timeout <= 30 and
            1 <= args.duration <= 43200):
        parser.error('interval 1..60, timeout 1..30, duration 1..43200 seconds')
    try:
        argv = json.loads(args.ssh_argv_file.read_text())
    except (OSError, ValueError):
        parser.error('cannot read valid SSH argv JSON')
    if not isinstance(argv, list) or not argv or not all(isinstance(a, str) and a for a in argv):
        parser.error('SSH argv must be a nonempty JSON list of strings')
    deadline = time.monotonic() + args.duration
    with args.output.open('x') as log:
        while time.monotonic() < deadline:
            row = probe(argv, min(args.timeout, deadline-time.monotonic()))
            line = json.dumps(row, sort_keys=True)
            log.write(line+'\n')
            log.flush()
            print(line, flush=True)
            if row['state'] == 'SSH_ONLINE' and args.stop_on_online:
                return
            time.sleep(min(args.interval, max(0, deadline-time.monotonic())))


if __name__ == '__main__':
    main()
