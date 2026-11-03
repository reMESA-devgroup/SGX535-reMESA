#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only actual source accounting tests, not physical qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
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
    fragment = a.module/'remesa_owned_fill_source.inc'
    code = source.read_text()
    generated = a.output/'owned_source_actual.inc'
    generated.write_text(extract(code, 'lost')+'\n'+fragment.read_text()+'\n'+'\n'.join(
        extract(code, n) for n in ('valid', 'remesa_stream_source_claim',
        'remesa_stream_source_release', 'remesa_stream_source_bind', 'remesa_route_source_producer'))+'\n')
    # The tested guard selection must actually feed all three runtime admission
    # sites; fail if a stager silently omits a hook. No generated source edit.
    assert 'owned_fill_pending(dev,a,d);' in extract(code, 'remesa_route_source_pending')
    assert 'source_lineage()' in extract(code, 'remesa_stream_source_settled')
    assert 'source_available()' in extract(code, 'remesa_stream_source_settled')
    assert 'o->boundary=*source_lineage();' in extract(code, 'remesa_stream_source_bind')
    variants = [('native', 'cc', ['-O2'], []),
                ('ubsan', 'cc', ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'], [])]
    if a.i486_cc:
        variants.append(('i486', str(a.i486_cc), ['-O2', '-static', '-march=i486'], [str(a.qemu_i386)]))
    test = w/'kernel/remesa/gma500/tests/test_owned_fill_source.c'
    rows = []
    for label, cc, flags, runner in variants:
        exe = a.output/label
        cmd = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DREMESA_GENERAL_SCENE', *flags,
               '-I'+str(a.output), '-I'+str(a.module), '-I'+str(w/'include'), str(test),
               str(a.module/'frozen_source_guard.c'), '-o', str(exe)]
        for name, argv in [(label+'-build', cmd)]+[(label+'-'+str(i), runner+[str(exe), str(i)]) for i in range(115)]:
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
                raise RuntimeError(name+': '+q.stderr.decode()[-2000:])
    files = [Path(__file__), test, source, fragment, generated,
             a.module/'remesa_owned_fill_source.h', a.module/'remesa_gem_twod_job.h',
             a.module/'frozen_source_guard.c', a.module/'frozen_source_guard.h']
    result = dict(status='OWNED_CLEAR_SOURCE_ACCOUNTING_OFFLINE_PASS', GPU_operations=0,
                  physical_qualification=False, production_calls_wired=True, installed=False,
                  scope='Actual owned source, original claim/producer/release/bind and frozen guard. Modeled kernel primitives and retirement receipts. Native/endpoint integration requires its separate joined tests and physical qualification.',
                  variants=len(variants), scenarios=115, commands=rows,
                  sources={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (a.output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(result['status'])


if __name__ == '__main__':
    main()
