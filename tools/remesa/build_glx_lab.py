#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Link private SGX-only Xlib GLX against the actual qualified Mesa archives.

Uses existing generated Mesa headers and bridge archive as immutable inputs.
No system install, software winsys, indirect GLX, DRM open or GPU execution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
from prepare_glx_lab import prepare

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cache', type=Path, required=True)
p.add_argument('--dependencies', type=Path, required=True)
p.add_argument('--bridge-build', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--ubsan', action='store_true')
a = p.parse_args()
r = Path(__file__).resolve().parents[2]
s, b = a.cache.resolve() / 'mesa-25.2.0', a.cache.resolve() / 'build-i386'
d = a.output.resolve()
prepare(s, d / 'overlay')
env = dict(os.environ)
env.update(json.loads((a.dependencies / 'build-environment.json').read_text()))
record = next(x for x in json.loads((b / 'compile_commands.json').read_text())
              if x['file'].endswith('/remesa_nir.c'))
raw = record.get('arguments') or shlex.split(record['command'])
cc, skip = [], False
for arg in raw:
    if skip:
        skip = False
        continue
    if arg in ('-o', '-MF', '-MQ'):
        skip = True
        continue
    if arg in ('-c', '-MD', '-DNDEBUG') or arg == record['file']:
        continue
    cc.append(arg)
root = a.dependencies.resolve() / 'root/usr'
cc += ['-UNDEBUG', '-DREMESA_WITH_MESA_HEADERS', '-DREMESA_WITH_NIR',
       '-DGLX_USE_TLS', '-DUSE_XSHM', '-DHAVE_X11_PLATFORM', '-fPIC']
cc += ['-I' + str(x) for x in (d / 'overlay', r / 'mesa/sgx535', r / 'tools',
       s / 'src/gallium/auxiliary', s / 'src/mesa', b / 'src/mesa',
       s / 'src/mapi', b / 'src/mapi', b / 'src/mapi/glapi/gen', root / 'include')]
if a.ubsan:
    cc += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
commands = []


def run(command, name):
    commands.append(command)
    (d / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
    result = subprocess.run(command, cwd=b, env=env, capture_output=True, text=True)
    (d / (name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(f'{name}: exit {result.returncode}; see {d}')


sources = [d / 'overlay' / (name + '.c') for name in
           ('glx_api', 'glx_getproc', 'glx_usefont', 'xm_api', 'xm_st', 'glx_lab_frontend')]
sources += [r / 'mesa/sgx535/remesa_xcb.c', r / 'mesa/sgx535/remesa_present_scale.c']
objects = []
for source in sources:
    obj = d / (source.stem + '.o')
    run(cc + ['-c', str(source), '-o', str(obj)], source.stem)
    objects.append(str(obj))
archives = ['util/libmesa_util_simd.a', 'util/libmesa_util.a', 'util/libxmlconfig.a',
            'loader/libloader.a', 'compiler/libcompiler.a', 'mesa/libmesa_sse41.a',
            'mesa/libmesa.a', 'gallium/auxiliary/libgallium.a',
            'gallium/auxiliary/libgalliumvl.a', 'compiler/nir/libnir.a',
            'compiler/spirv/libvtn.a', 'compiler/glsl/libglsl.a',
            'compiler/glsl/glcpp/libglcpp.a', 'mapi/shared-glapi/libglapi.a',
            'util/blake3/libblake3.a', 'c11/impl/libmesa_util_c11.a',
            'gallium/drivers/sgx535/libremesa_loader.a']
archives = [b / 'src' / x for x in archives]
bridge = a.bridge_build.resolve() / 'src/mapi/glapi/libglapi_bridge.a'
version = d / 'exports.sym'
version.write_text('{global: gl*; local: *;};\n')
run(cc + ['-shared', '-Wl,-z,defs', '-Wl,-Bsymbolic', '-Wl,-z,noexecstack',
    '-Wl,--version-script=' + str(version), '-Wl,-soname,libGL-remesa-lab.so.1'] + objects +
    ['-Wl,--whole-archive', str(bridge), '-Wl,--no-whole-archive', '-Wl,--start-group'] +
    list(map(str, archives)) + ['-Wl,--end-group', '-L' + str(root / 'lib/i386-linux-gnu'),
    '-L' + str(a.cache.resolve() / 'i386-deps/usr/lib/i386-linux-gnu'),
    '-lX11-xcb', '-lX11', '-lXext', '-lxcb', '-ldrm', '-lm', '-ldl', '-latomic', '-pthread',
    str(a.cache.resolve() / 'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),
    '-o', str(d / 'libGL-remesa-lab.so.1')], 'link')
probe = r / 'tools/remesa/test_glx_lab_window.c'
run(cc + [str(probe), '-L' + str(d), '-l:libGL-remesa-lab.so.1',
    '-L' + str(root / 'lib/i386-linux-gnu'), '-lX11', '-lxcb',
    '-Wl,-rpath-link,' + str(root / 'lib/i386-linux-gnu'),
    '-Wl,-rpath-link,' + str(a.cache.resolve() / 'i386-deps/usr/lib/i386-linux-gnu'),
    '-Wl,-rpath,$ORIGIN', '-o', str(d / 'test-glx-window')], 'probe-link')
inputs = sources + archives + [bridge, probe]
(d / 'INPUT_SHA256.json').write_text(json.dumps(
    {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}, indent=2) + '\n')
(d / 'BUILD_RESULTS.json').write_text(json.dumps({'status': 'BUILD_PASS',
    'GPU': False, 'software_winsys': False, 'ubsan_frontend': a.ubsan,
    'driver_archive': str(archives[-1])}, indent=2) + '\n')
