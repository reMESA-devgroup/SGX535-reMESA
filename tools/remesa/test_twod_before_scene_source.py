#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproduce the physical 2D-before-3D source refusal with actual C authority.

CPU-only source/lock model, not GPU retirement or a proposed gate relaxation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import resource
from test_private_source_mmio import extract


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--i486-cc', type=Path)
    p.add_argument('--qemu-i386', type=Path)
    a = p.parse_args()
    if bool(a.i486_cc) != bool(a.qemu_i386):
        p.error('both i486 tools are required')
    a.output.mkdir(parents=True, exist_ok=False)
    w = Path(__file__).resolve().parents[2]
    source = a.module/'remesa_stream_source.c'
    generated = a.output/'twod_source_actual.inc'
    generated.write_text('\n'.join(extract(source.read_text(), n) for n in
        ('lost', 'remesa_stream_source_claim', 'remesa_route_source_producer'))+'\n')
    variants = [('native', 'cc', ['-O2'], []),
                ('ubsan', 'cc', ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'], [])]
    if a.i486_cc:
        variants.append(('i486', str(a.i486_cc), ['-O2', '-static', '-march=i486'], [str(a.qemu_i386)]))
    test = w/'kernel/remesa/gma500/tests/test_twod_before_scene_source.c'
    rows = []
    for label, cc, flags, runner in variants:
        exe = a.output/label
        cmd = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
               '-I'+str(a.output), '-I'+str(a.module), str(test),
               str(a.module/'frozen_source_guard.c'), '-o', str(exe)]
        for name, argv in [(label+'-build', cmd)]+[(label+'-'+str(i), runner+[str(exe), str(i)]) for i in range(4)]:
            def limit():
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                n = 6*1024**3 if runner else 768*1024**2
                resource.setrlimit(resource.RLIMIT_AS, (n, n))
            q = subprocess.run(argv, capture_output=True, timeout=60, preexec_fn=limit)
            (a.output/(name+'.stdout')).write_bytes(q.stdout)
            (a.output/(name+'.stderr')).write_bytes(q.stderr)
            rows.append(dict(label=name, argv=argv, exit=q.returncode))
            (a.output/'COMMANDS.json').write_text(json.dumps(rows, indent=2)+'\n')
            if q.returncode or q.stderr:
                raise RuntimeError(name+': '+q.stderr.decode()[-1500:])
    files = [Path(__file__), test, source, generated, a.module/'frozen_source_guard.c', a.module/'frozen_source_guard.h']
    result = dict(status='ACTUAL_FIRST_OWNER_SOURCE_REFUSAL_REPRODUCED', GPU_operations=0,
                  physical_qualification=False, variants=len(variants), scenarios=4, commands=rows,
                  sources={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},
                  finding='Fresh claim works; any producer interval invalidates lineage. Ending producer does not repair it. Claiming before 2D alone also fails. No guard bypass or driver change.')
    (a.output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(result['status'])


if __name__ == '__main__':
    main()
