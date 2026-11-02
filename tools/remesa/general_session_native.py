#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Supervise two closed native jobs from a real API draw; default plan only.

Does not enable gates, install, reboot, retry or present. Physical boot/module
and source/recovery qualification belongs to the outer experiment controller.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import time
import private_probe as p
import gears_scene_native as native
import gears_scene_profile as scene
import verify_general_session_capture as archive


def collect(device, output, catalogue, size, client, client_sha, oracle,
            oracle_sha, controls, controls_sha, plan_trace, plan_trace_sha, asset_root=None):
    archive.need(p.sha(client.read_bytes()) == client_sha, 'client identity changed')
    plan, _ = scene.load_profile(catalogue, size, asset_root, clamped_api=True)
    entry = next(x for x in json.loads(catalogue.read_text())['profiles'] if x['size'] == size)
    fixture = asset_root/size if asset_root else Path(entry['packet_path']).parent
    report = dict(status='PREPARING', identity=p.device_identity(device),
                  native_client_sha256=client_sha, reference_images_uploaded_by_supervisor=False,
                  API_draws=1, requested_GPU_jobs=2, physical_qualification=False,
                  requires_outer_boot_source_recovery_audit=True)
    def save():
        (output/'SUPERVISOR-CAPTURE.json').write_text(json.dumps(report, indent=2)+'\n')
    save()
    fd = os.open(device, os.O_RDWR | os.O_CLOEXEC)
    memory, remaining, complete = None, [], False
    try:
        report['status'] = 'CLIENT_MAY_HAVE_ENTERED';save()
        start = time.monotonic()
        with (output/'native.stdout').open('xb') as stdout, (output/'native.stderr').open('xb') as stderr:
            q = subprocess.run([str(client), str(fd), size, str(output), str(fixture), '--session-2'],
                               stdout=stdout, stderr=stderr, pass_fds=(fd,), timeout=180)
        report['native_client_exit'] = q.returncode
        report['wall_seconds_including_LAB_reads'] = time.monotonic()-start
        archive.need(q.returncode == 0, 'native client failed; no retry/cleanup')
        checked = archive.verify(output, catalogue, size, oracle, oracle_sha,
                                 controls, controls_sha, plan_trace, plan_trace_sha, asset_root)
        report['archive'] = checked
        opened = (output/'OPEN-REQUEST.bin').read_bytes()
        context = struct.unpack_from('<Q', opened, 8)[0]
        ids = struct.unpack_from('<18I', opened, 32)
        before = native.inventory((output/'INVENTORY-BEFORE.bin').read_bytes(), context, 24)
        owners = native.inventory((output/'INVENTORY-OWNERS.bin').read_bytes(), context, 20)
        byid = {e[2]: e for e in before}
        archive.need(all(byid.get(e[2]) == e for e in owners), 'source/CPU-owner backing differs')
        for i, binding in enumerate(plan['bindings']):
            archive.need(byid.get(ids[i]) == (binding['va'], binding['bytes'], ids[i], 1), 'closed backing ownership')
        guard_ids = struct.unpack('<4I', (output/'GUARD_IDS.bin').read_bytes())
        guard_va = struct.unpack('<4Q', (output/'GUARD_VA.bin').read_bytes())
        archive.need({e[2] for e in owners} | set(guard_ids) == set(byid), 'unaccounted allocations')
        expected_guards = []
        for lo, hi in [(0x20000000, 0x28000000), (0x30000000, 0x40000000), (0x40000000, 0xe0000000)]:
            group = [e for e in owners if lo <= e[0] < hi]
            if group:
                for va in [min(e[0] for e in group)-4096, max(e[0]+e[1] for e in group)]:
                    if lo <= va and va+4096 <= hi:
                        expected_guards.append(va)
        archive.need(list(guard_va) == expected_guards and len(set(guard_ids)) == 4, 'guards do not enclose graph')
        for ident, va in zip(guard_ids, guard_va):
            archive.need(byid.get(ident) == (va, 4096, ident, 1), 'guard backing')
        remaining = native.inventory((output/'INVENTORY-REMAINING.bin').read_bytes(), context, 6)
        expected = set(guard_ids) | {ids[plan['color_index']], ids[plan['depth_index']]}
        archive.need({e[2] for e in remaining} == expected and all(byid[e[2]] == e for e in remaining), 'remaining ownership')
        memory = p.Memory(fd, context);memory.ids = expected
        info = memory.info();report['provider_info'] = list(info)
        archive.need(info[3:5] == (6, 6) and not any(info[k] for k in [6, 7, 8]), 'remaining endpoint not idle')
        report['object_info'] = []
        for va, n, ident, _ in remaining:
            info = memory.info(ident);report['object_info'].append(dict(id=ident, info=list(info)))
            archive.need(info[1:3] == (n, va) and not any(info[k] for k in [6, 7, 8, 11])
                         and info[9] == 1 and info[10] == info[12] == n//4096, 'remaining object ownership')
        complete = True;report['status'] = 'TWO_JOB_CAPTURE_MATCHES_ORACLES_OUTER_AUDIT_PENDING'
    except BaseException as error:
        report['status'] = 'FAILED';report['error'] = type(error).__name__+': '+str(error)
        raise
    finally:
        if complete and memory is not None and not memory.poisoned:
            try:
                for _, _, ident, _ in reversed(remaining):
                    memory.request(p.UNMAP, ident);memory.request(p.UNPIN, ident);memory.request(p.DESTROY, ident)
                memory.request(p.CLOSE);report['memory_cleanup'] = 'complete'
            except Exception as error:
                report['status'] = 'FAILED';report['memory_cleanup'] = 'failed';report['cleanup_error'] = str(error)
        else:
            report['memory_cleanup'] = 'suppressed_after_uncertain_or_invalid_evidence'
        try:
            os.close(fd)
        finally:
            save()
    archive.need(report.get('memory_cleanup') == 'complete', 'incomplete memory cleanup')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('catalogue', 'client', 'oracle', 'controls', 'plan-trace', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    for name in ('client-sha256', 'oracle-sha256', 'controls-sha256', 'plan-trace-sha256'):
        parser.add_argument('--'+name, required=True)
    parser.add_argument('--size', required=True, choices=['1280x800', '640x640'])
    parser.add_argument('--device', type=Path, default=Path('/dev/dri/card0'))
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--asset-root', type=Path)
    a = parser.parse_args()
    plan, _ = scene.load_profile(a.catalogue, a.size, a.asset_root, clamped_api=True)
    a.output.mkdir(parents=True, exist_ok=False)
    if not a.execute:
        (a.output/'PLAN.json').write_text(json.dumps(dict(plan, mode='ONE_API_DRAW_TWO_NATIVE_JOBS',
                                                        physical_qualification=False), indent=2)+'\n')
        print('PLAN_ONLY_NO_DEVICE_OPEN');return
    result = collect(a.device, a.output, a.catalogue, a.size, a.client.resolve(), a.client_sha256,
                     a.oracle, a.oracle_sha256, a.controls, a.controls_sha256,
                     a.plan_trace, a.plan_trace_sha256, a.asset_root)
    print(result['status'])


if __name__ == '__main__':
    main()
