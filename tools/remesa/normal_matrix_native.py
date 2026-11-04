#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Supervise bounded ordinary OpenGL draws; default plan only, no gate/boot changes.

Requires a separately qualified boot/controller. Verifies presealed inputs,
retirement, pixels and remaining ownership before manual GEM cleanup. Never
uploads expected framebuffer images, retries a draw or presents CPU pixels.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import private_probe as memory_api
import verify_normal_matrix_capture as archive
import verify_normal_preserve_capture as preserve_archive


def collect(device, output, fixture, oracle, size, manifest, manifest_sha, client, client_sha, draws=1, preserve=False):
    archive.need(draws == 3 if preserve else draws in (1, 2, 10, 100), 'bounded ordinary draw count')
    if preserve:
        archive.need(manifest == fixture/'MANIFEST.json' and oracle == fixture/'oracle', 'preserve input paths')
        preserve_archive.preflight(fixture, size, manifest_sha)
    else:
        archive.preflight(fixture, oracle, size, manifest, manifest_sha)
    archive.need(archive.sha(client.read_bytes()) == client_sha, 'native client prelaunch pin')
    report = dict(status='PREPARING', identity=memory_api.device_identity(device),
                  native_client_sha256=client_sha, physical_qualification=False,
                  requires_outer_boot_source_recovery_audit=True,
                  requested_API_draws=draws, requested_3D_jobs=draws, requested_2D_fills=2 if preserve else draws,
                  preserve_candidate=preserve,
                  reference_framebuffers_uploaded_to_GPU=False)

    def save():
        (output/'SUPERVISOR-CAPTURE.json').write_text(json.dumps(report, indent=2)+'\n')

    save()
    fd = os.open(device, os.O_RDWR | os.O_CLOEXEC)
    memory, remaining, complete = None, [], False
    try:
        report['status'] = 'CLIENT_MAY_HAVE_ENTERED'
        save()
        start = time.monotonic()
        with (output/'native.stdout').open('xb') as stdout, (output/'native.stderr').open('xb') as stderr:
            argv = [str(client), str(fd), size, str(output), str(fixture), '--execute']
            if draws != 1:
                argv.append(str(draws))
            result = subprocess.run(argv, pass_fds=(fd,), stdout=stdout, stderr=stderr,
                                    timeout=600 if draws == 100 else 180)
        report['native_client_exit'] = result.returncode
        report['wall_seconds_including_diagnostics'] = time.monotonic()-start
        archive.need(result.returncode == 0, 'native client failed; no retry/manual cleanup')
        checked = (preserve_archive.verify(output, fixture, size, manifest_sha) if preserve else
                   archive.verify_continuous(output, fixture, oracle, size, manifest, manifest_sha, draws)
                   if draws != 1 else archive.verify(output, fixture, oracle, size, manifest, manifest_sha))
        report['archive'] = checked
        remaining = checked['remaining']
        memory = memory_api.Memory(fd, checked['context'])
        memory.ids = {e[2] for e in remaining}
        info = memory.info()
        report['provider_info'] = list(info)
        archive.need(info[3:5] == (6, 6) and not any(info[k] for k in (6, 7, 8)), 'remaining endpoint not idle')
        report['object_info'] = []
        for va, n, ident, _ in remaining:
            info = memory.info(ident)
            report['object_info'].append(dict(id=ident, info=list(info)))
            archive.need(info[1:3] == (n, va) and info[9] == 1 and info[10] == info[12] == n//4096
                         and not any(info[k] for k in (6, 7, 8, 11)), 'remaining mapping/lifetime mismatch')
        complete = True
        report['status'] = 'ORDINARY_API_CAPTURE_VALID_OUTER_PHYSICAL_AUDIT_PENDING'
    except BaseException as error:
        report['status'] = 'FAILED'
        report['error'] = type(error).__name__+': '+str(error)
        raise
    finally:
        if complete and memory is not None and not memory.poisoned:
            try:
                for _, _, ident, _ in reversed(remaining):
                    memory.request(memory_api.UNMAP, ident)
                    memory.request(memory_api.UNPIN, ident)
                    memory.request(memory_api.DESTROY, ident)
                memory.request(memory_api.CLOSE)
                report['memory_cleanup'] = 'complete'
            except Exception as error:
                report.update(status='FAILED', memory_cleanup='failed', cleanup_error=str(error))
        else:
            report['memory_cleanup'] = 'suppressed_after_uncertain_or_invalid_evidence'
        try:
            os.close(fd)
        finally:
            save()
    archive.need(report.get('memory_cleanup') == 'complete', 'incomplete memory cleanup')
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'fixture', 'oracle', 'manifest', 'client'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--size', choices=['1280x800', '640x640'], required=True)
    p.add_argument('--manifest-sha256', required=True)
    p.add_argument('--client-sha256', required=True)
    p.add_argument('--device', type=Path, default=Path('/dev/dri/card0'))
    p.add_argument('--execute', action='store_true')
    p.add_argument('--draws', type=int, choices=(1, 2, 3, 10, 100), default=1)
    p.add_argument('--preserve-three-step', action='store_true')
    a = p.parse_args()
    if a.preserve_three_step:
        archive.need(a.draws == 3 and a.manifest == a.fixture/'MANIFEST.json' and
                     a.oracle == a.fixture/'oracle', 'bounded preserve inputs')
        preserve_archive.preflight(a.fixture, a.size, a.manifest_sha256)
    else:
        archive.need(a.draws in (1, 2, 10, 100), 'bounded ordinary draw count')
        archive.preflight(a.fixture, a.oracle, a.size, a.manifest, a.manifest_sha256)
    archive.need(archive.sha(a.client.read_bytes()) == a.client_sha256, 'native client prelaunch pin')
    a.output.mkdir(parents=True, exist_ok=False)
    if not a.execute:
        (a.output/'PLAN.json').write_text(json.dumps(dict(status='PLAN_ONLY_NO_DEVICE_OPEN',
            GPU_operations=0, size=a.size, qualification_manifest_sha256=a.manifest_sha256,
            client_sha256=a.client_sha256, requested_3D_jobs=a.draws, requested_2D_fills=2 if a.preserve_three_step else a.draws,
            preserve_candidate=a.preserve_three_step), indent=2)+'\n')
        print('PLAN_ONLY_NO_DEVICE_OPEN')
        return
    print(collect(a.device, a.output, a.fixture, a.oracle, a.size, a.manifest,
                  a.manifest_sha256, a.client, a.client_sha256, a.draws, a.preserve_three_step)['status'])


if __name__ == '__main__':
    main()
