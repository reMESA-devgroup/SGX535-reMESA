#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual private loan publication plus general DPM cache transitions; no GPU."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import stage_twod_reservation as stage


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native', required=True, type=Path)
    p.add_argument('--shim', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--i486-cc', type=Path)
    p.add_argument('--qemu-i386', type=Path)
    a = p.parse_args()
    if bool(a.i486_cc) != bool(a.qemu_i386):
        p.error('both i486 options required')
    w = Path(__file__).resolve().parents[2]
    native, out = a.native.resolve(), a.output.resolve()
    raw = (native/'remesa_gem_memory.c').read_bytes()
    if hashlib.sha256(raw).hexdigest() != stage.PINNED['remesa_gem_memory.c']:
        raise ValueError('unreviewed native memory source')
    out.mkdir(parents=True, exist_ok=False)
    source, header = stage.job_capacity_sources(raw.decode(), (native/'remesa_gem_memory.h').read_text())
    source = stage.private_file_lost_sources(stage.dpm_origin_source(source))
    # The rich private-loan fixture includes real session/seal helpers after
    # defining its MMU callback types. Source/executor functions are not used.
    for name in ['remesa_private_memory_file_lost.inc', 'remesa_private_session.inc',
                 'remesa_private_session_dpm.inc', 'remesa_private_session_source.inc',
                 'remesa_private_session_scene.inc', 'remesa_private_inputs.inc']:
        source = stage.replace_once(source, '#include "'+name+'"\n', '')
    (out/'remesa_gem_memory.c').write_text(source+'\n#include "remesa_gem_dpm.inc"\n')
    (out/'remesa_gem_memory.h').write_text(header)
    variants = [('native', 'cc', ['-O2'], []),
                ('ubsan', 'cc', ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'], [])]
    if a.i486_cc:
        variants.append(('i486', str(a.i486_cc), ['-O2', '-static', '-march=i486'], [str(a.qemu_i386)]))
    test = w/'kernel/remesa/gma500/tests/test_private_dpm_cache.c'
    rows = []
    for name, cc, flags, runner in variants:
        exe = out/name
        command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-Wno-missing-field-initializers', '-ffunction-sections', '-fdata-sections',
                   '-Wl,--gc-sections', *flags, '-I'+str(out), '-I'+str(a.shim.resolve().parent),
                   '-I'+str(native), '-I'+str(w/'include'), '-I'+str(w/'kernel/remesa/gma500'),
                   '-I'+str(w/'mesa/sgx535'),
                   str(test), '-o', str(exe)]
        for label, argv in [(name+'-build', command), (name+'-run', runner+[str(exe)])]:
            def limit():
                n = 6*1024**3 if runner and label.endswith('-run') else 768*1024**2
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                resource.setrlimit(resource.RLIMIT_AS, (n, n))
            q = subprocess.run(argv, capture_output=True, text=True, timeout=120, preexec_fn=limit)
            (out/(label+'.log')).write_text(q.stdout+q.stderr)
            rows.append(dict(label=label, argv=argv, exit=q.returncode))
            (out/'COMMANDS.json').write_text(json.dumps(rows, indent=2)+'\n')
            if q.returncode or q.stderr:
                raise RuntimeError(label+': '+q.stderr[-4000:])
    files = [Path(__file__).resolve(), Path(stage.__file__).resolve(), test, a.shim.resolve(),
             native/'remesa_gem_memory.c', out/'remesa_gem_memory.c', out/'remesa_gem_memory.h']
    files += [w/'kernel/remesa/gma500'/n for n in ['remesa_private_dpm_cache.inc',
              'remesa_gem_private.inc', 'remesa_gem_dpm.inc', 'remesa_private_session.inc',
              'remesa_private_session_dpm.inc', 'remesa_private_session.h', 'remesa_gem_job_limits.h',
              'tests/test_gem_private.c', 'tests/test_gem_twod.c']]
    files += [w/'mesa/sgx535/remesa_twod.c', w/'mesa/sgx535/remesa_twod.h']
    result = dict(status='ACTUAL_GENERAL_DPM_CACHE_PUBLICATION_PASS', GPU_operations=0,
                  scope='Actual memory/private validation/publication/cache/seal/release. Injected MMU/GEM/cache primitives; completion boundary modeled. Generic publication jobs confer no general-scene warm admission.',
                  pool_flushes=[2, 1, 0, 2], final_unseal_flushes=2,
                  extra_cleared_pages=0, commands=rows,
                  sources={str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'status': result['status'], 'variants': len(variants), 'GPU_operations': 0}))


if __name__ == '__main__':
    main()
