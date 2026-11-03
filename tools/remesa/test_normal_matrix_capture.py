#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline actual-client archive fault tests and supervisor lifecycle models.

Original ioctl-model captures MUST fail pixel verification. A separate test
copy receives CPU expectation bytes solely to test verifier acceptance/faults;
that copy and its receipts are not physical evidence or GPU rasterization.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
from types import SimpleNamespace
from unittest import mock
import normal_matrix_native as supervisor
import normal_matrix_oracle as oracle
import verify_normal_matrix_capture as archive


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--supervisor-draws', type=int, choices=(1, 2, 10, 100), default=1)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    result = dict(GPU_operations=0, physical_qualification=False, sizes=[], faults=0, lifecycle=[])
    read = Path.read_bytes

    def refused(fn):
        try:
            fn()
        except (ValueError, FileNotFoundError):
            return
        raise AssertionError('fault accepted')

    for size in ('1280x800', '640x640'):
        fixture = a.root/'producer-v2'/size
        expectation = a.root/('oracle-'+size)
        manifest = a.root/('MANIFEST-'+size+'.json')
        digest = oracle.sha(manifest.read_bytes())
        args = (fixture, expectation, size, manifest, digest)
        original = a.root/'client-model'/size
        refused(lambda: archive.verify(original, *args))  # Zero model framebuffer is not a physical pass.
        capture = a.output/('SYNTHETIC-NOT-GPU-'+size)
        shutil.copytree(original, capture, copy_function=os.link)

        def replace(name, data):
            dest = capture/name
            if dest.exists():
                dest.unlink()  # Break test copy's hardlink BEFORE writing.
            dest.write_bytes(data)

        packet = (capture/'packet.bin').read_bytes()
        color, depth = struct.unpack_from('<II', packet, 352)
        for ident, name in ((color, 'COLOR.bin'), (depth, 'Z24.bin')):
            replace(f'after-id-{ident}.bin', (expectation/name).read_bytes())
        width, height = map(int, size.split('x'))
        color_bytes = (expectation/'COLOR.bin').read_bytes()
        replace('GL-READPIXELS-BGRA.bin', b''.join(color_bytes[y*width*4:(y+1)*width*4] for y in range(height-1, -1, -1)))
        replace('SYNTHETIC-CLASSIFICATION.json', b'{"GPU_operations":0,"physical_qualification":false,"pixels":"CPU expectation copied only for verifier fault tests"}\n')
        checked = archive.verify(capture, *args)
        cached = archive.preflight(*args)
        # Fixtures/oracles remain immutable and were fully preflighted above.
        # Cache only those unchanged preflight inputs during envelope mutation.
        with mock.patch.object(archive, 'preflight', return_value=cached):
            for name, offsets in (
                ('MATRIX-RESPONSE.bin', range(2344)),
                ('MATRIX-RECEIPT.bin', range(20)),
                ('MATRIX-HOST-OP.bin', range(16)),
                ('FILL-RESPONSE.bin', [i for i in range(104) if not 72 <= i < 80]),
                ('FILL-REQUEST.bin', range(104))):
                target = capture/name
                old = read(target)
                for i in offsets:
                    changed = bytearray(old)
                    changed[i] ^= 1

                    def intercept(path, target=target, changed=bytes(changed)):
                        return changed if path == target else read(path)

                    with mock.patch.object(Path, 'read_bytes', intercept):
                        refused(lambda: archive.verify(capture, *args))
                    result['faults'] += 1
            guards = struct.unpack('<4I', read(capture/'GUARD_IDS.bin'))
            for name in ('GL-READPIXELS-BGRA.bin', f'after-id-{color}.bin', f'after-id-{depth}.bin',
                         f'after-id-{guards[0]}.bin', 'INFO-RETURN.bin', 'INVENTORY-REMAINING.bin',
                         'OPENGL-REPORT.bin', 'RESULT.bin'):
                target = capture/name
                old = read(target)
                for i in (0, len(old)//2, len(old)-1):
                    # Z24X8's unconsumed high byte is deliberately ignored.
                    if name == f'after-id-{depth}.bin' and i % 4 == 3:
                        continue
                    changed = bytearray(old)
                    changed[i] ^= 1
                    with mock.patch.object(Path, 'read_bytes', lambda path, target=target, changed=bytes(changed): changed if path == target else read(path)):
                        refused(lambda: archive.verify(capture, *args))
                    result['faults'] += 1
        # Real signature/fixture preflight is never bypassed for pin tests.
        refused(lambda: archive.preflight(fixture, expectation, size, manifest, '0'*64))
        result['sizes'].append(dict(size=size, original_zero_capture_refused=True,
                                   synthetic_verifier_positive=True, operation=checked['operation']))
        if size != '1280x800':
            continue
        client = a.root/'client-final/opengl_matrix_native_client'
        client_sha = oracle.sha(client.read_bytes())
        for pin in ('manifest', 'client'):
            with mock.patch.object(supervisor.os, 'open') as opened:
                refused(lambda: supervisor.collect(Path('/no-real-device'), a.output,
                    fixture, expectation, size, manifest, '0'*64 if pin == 'manifest' else digest,
                    client, '0'*64 if pin == 'client' else client_sha))
                opened.assert_not_called()
            result['lifecycle'].append(dict(fault=pin+'_pin', device_open=False))
        for fault in ('ok', 'client', 'timeout', 'archive', 'global_info', 'object_info', 'unmap', 'unpin', 'destroy', 'close'):
            requests = []
            instances = []
            byid = {e[2]:e for e in checked['remaining']}

            class Memory:
                poisoned = False

                def __init__(self, fd, context):
                    assert fd == 99 and context == checked['context']
                    instances.append(self)

                def info(self, ident=0):
                    info = [sum(e[1] for e in byid.values()), 0, 0, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0]
                    if ident:
                        va, length, _, _ = byid[ident]
                        info[1:3] = [length, va]
                        info[9], info[10], info[12] = 1, length//4096, length//4096
                    if fault == ('object_info' if ident else 'global_info'):
                        info[7] = 1
                    return tuple(info)

                def request(self, action, ident=0):
                    requests.append((action, ident))
                    if fault == {4:'unmap', 5:'unpin', 6:'destroy', 13:'close'}.get(action):
                        raise RuntimeError('modeled cleanup failure')

            dest = a.output/('lifecycle-'+fault)
            dest.mkdir()
            run = mock.Mock(return_value=SimpleNamespace(returncode=1 if fault == 'client' else 0))
            if fault == 'timeout':
                run.side_effect = subprocess.TimeoutExpired('modeled client', 180)
            verify = mock.Mock(return_value=checked)
            if fault == 'archive':
                verify.side_effect = ValueError('modeled invalid evidence')
            with mock.patch.object(supervisor.memory_api, 'device_identity', return_value={'modeled':True}), \
                 mock.patch.object(supervisor.os, 'open', return_value=99) as opened, \
                 mock.patch.object(supervisor.os, 'close') as closed, \
                 mock.patch.object(supervisor.subprocess, 'run', run), \
                 mock.patch.object(supervisor.memory_api, 'Memory', Memory), \
                 mock.patch.object(supervisor.archive, 'verify' if a.supervisor_draws == 1 else 'verify_continuous', verify):
                try:
                    report = supervisor.collect(Path('/no-real-device'), dest, *args, client, client_sha, a.supervisor_draws)
                    assert fault == 'ok' and report['memory_cleanup'] == 'complete'
                except (ValueError, RuntimeError, subprocess.TimeoutExpired):
                    assert fault != 'ok'
                opened.assert_called_once()
                closed.assert_called_once_with(99)
                assert run.call_count == 1
                command = run.call_args.args[0]
                assert (command[-1] == '--execute') if a.supervisor_draws == 1 else (command[-2:] == ['--execute', str(a.supervisor_draws)])
            stored = json.loads((dest/'SUPERVISOR-CAPTURE.json').read_text())
            if fault in ('client', 'timeout', 'archive', 'global_info', 'object_info'):
                assert not requests and stored['memory_cleanup'] == 'suppressed_after_uncertain_or_invalid_evidence'
            if fault in ('unmap', 'unpin', 'destroy'):
                assert len(requests) == {'unmap':1, 'unpin':2, 'destroy':3}[fault]
            if fault in ('ok', 'close'):
                assert len(requests) == 19 and requests[-1] == (13, 0)
            result['lifecycle'].append(dict(fault=fault, requests=len(requests), status=stored['status']))
    files = [Path(__file__), Path(archive.__file__), Path(supervisor.__file__), Path(oracle.__file__)]
    result.update(status='ORDINARY_MATRIX_ARCHIVE_AND_SUPERVISOR_MODELS_PASS',
                  sources={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (a.output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:v for k, v in result.items() if k not in ('sources', 'lifecycle')}))


if __name__ == '__main__':
    main()
