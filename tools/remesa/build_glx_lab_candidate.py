#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build, never execute, the explicit one-attempt retained GLX LAB caller.

Input is an already built private frontend from build_glx_lab.py. The external
controller must separately protect the new executable AND its dynamic library
and bind a new execution card. Old authorizations/hashes are not reusable.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--frontend-build', type=Path, required=True)
p.add_argument('--cache', type=Path, required=True)
p.add_argument('--dependencies', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
r = Path(__file__).resolve().parents[2]
d = a.output.resolve()
d.mkdir(parents=True, exist_ok=False)
commands = json.loads((a.frontend_build / 'COMMANDS.json').read_text())
cc = commands[0][:commands[0].index('-c')]
group = next(command for command in commands if '-shared' in command)
archives = group[group.index('-Wl,--start-group')+1:group.index('-Wl,--end-group')]
root = a.dependencies.resolve() / 'root/usr/lib/i386-linux-gnu'
sources = [r / 'tools/remesa' / x for x in ('retained_fragment_probe_main.c',
           'retained_fragment_capture.c', 'glx_lab_triangle.c')]
command = cc + ['-I' + str(r / 'tools/remesa'), '-DREMESA_GLX_LAB_FRONTEND'] + \
    list(map(str, sources)) + ['-L' + str(a.frontend_build.resolve()),
    '-l:libGL-remesa-lab.so.1', '-Wl,--start-group'] + archives + \
    ['-Wl,--end-group', '-L' + str(root),
     '-L' + str(a.cache.resolve() / 'i386-deps/usr/lib/i386-linux-gnu'),
     '-Wl,-rpath-link,' + str(root), '-lX11', '-lxcb', '-ldrm', '-ldl', '-lm',
     '-latomic', '-pthread', '-Wl,-z,noexecstack', '-Wl,-rpath,$ORIGIN',
     str(a.cache.resolve() / 'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),
     '-o', str(d / 'retained-glx-once')]
(d / 'COMMAND.json').write_text(json.dumps(command, indent=2) + '\n')
env = dict(os.environ)
env.update(json.loads((a.dependencies / 'build-environment.json').read_text()))
result = subprocess.run(command, cwd=a.cache / 'build-i386', env=env,
                        capture_output=True, text=True)
(d / 'compile.log').write_text(result.stdout + result.stderr)
inputs = sources + [a.frontend_build / 'libGL-remesa-lab.so.1']
(d / 'BUILD_RESULTS.json').write_text(json.dumps({'exit': result.returncode,
    'GPU_executed': False, 'qualification': 'UNQUALIFIED_NOT_EXECUTED',
    'input_sha256': {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}}, indent=2) + '\n')
raise SystemExit(result.returncode)
