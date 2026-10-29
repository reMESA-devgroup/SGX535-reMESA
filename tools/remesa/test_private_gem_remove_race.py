#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproduce unjoined/late GEM destruction using actual staged function bodies.

This is a CPU counterexample with modeled DRM/GTT APIs, not a driver safety pass.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from test_private_source_mmio import extract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    module = args.module.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[2]
    test = root / 'kernel/remesa/gma500/tests/test_private_gem_remove_race.c'
    inputs = [module / 'gem.c', module / 'psb_drv.c', module / 'gtt.c', test, Path(__file__).resolve(),
              root / 'tools/remesa/test_private_source_mmio.py']
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    (output / 'gem_free_actual.inc').write_text(extract(inputs[0].read_text(), 'psb_gem_free_object')+'\n')
    (output / 'remove_actual.inc').write_text(extract(inputs[1].read_text(), 'psb_pci_remove')+'\n')
    (output / 'gtt_free_actual.inc').write_text(extract(inputs[2].read_text(), 'psb_gtt_free_range')+'\n')
    rows = []

    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (768*1024**2,)*2)

    for name, flags in [('native', ['-O2']), ('ubsan', ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'])]:
        binary = output / name
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', *flags,
                   '-I'+str(output), str(test), '-pthread', '-o', str(binary)]
        for label, argv in [(name+'-build', command), (name+'-run', [str(binary)])]:
            result = subprocess.run(argv, capture_output=True, text=True, timeout=60, preexec_fn=limits)
            (output / (label+'.log')).write_text(result.stdout+result.stderr)
            rows.append(dict(label=label, argv=argv, exit=result.returncode))
            if result.returncode or result.stderr:
                raise RuntimeError(label+': '+result.stderr)
    if hashes != {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}:
        raise RuntimeError('inputs changed during counterexample')
    report = dict(status='CURRENT_PRIVATE_GEM_REMOVAL_COUNTEREXAMPLE_REPRODUCED',
                  driver_safety_pass=False, GPU_operations=0,
                  scope='Actual destructor/GTT-free/remove bodies; mmap-pinned object; DRM/unpin/teardown APIs modeled, deterministic pthread ordering',
                  witnessed_cases=['destructor starts before unplug and reaches GTT after unload',
                                   'destructor starts after completed remove and reaches GTT after unload'],
                  control='destruction during unregister reaches GTT before unload',
                  sources=hashes, commands=rows)
    (output / 'RESULT.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: report[k] for k in ['status', 'driver_safety_pass', 'GPU_operations']}))


if __name__ == '__main__':
    main()
