#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture real Mesa fixed-function NIR using an explicit refuse-submit device.

A copied LAB frontend is relinked with a diagnostic-only NIR wrapper. This is
not original glxgears execution or hardware qualification. No driver source,
Mesa cache, installed library, display configuration or gate is changed.
"""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--frontend-build', type=Path, required=True)
p.add_argument('--cache', type=Path, required=True)
p.add_argument('--runtime-lib', type=Path, required=True)
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--display', required=True, help='Explicit disposable X11 display')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--ubsan', action='store_true')
a = p.parse_args()
d = a.output.resolve(); d.mkdir(parents=True, exist_ok=False)
r = Path(__file__).resolve().parent
c = a.cache.resolve(); b = c / 'build-i386'
env = dict(os.environ)
env.update(json.loads((c / 'build-environment.json').read_text()))
commands = json.loads((a.frontend_build / 'COMMANDS.json').read_text())
base = commands[0][:commands[0].index('-c')]
extra = ['-fsanitize=undefined', '-fno-sanitize-recover=undefined'] if a.ubsan else []
actual = []
def run(command, name, run_env=env):
    actual.append(command)
    (d / 'COMMANDS.json').write_text(json.dumps(actual, indent=2) + '\n')
    z = subprocess.run(command, cwd=b, env=run_env, capture_output=True, text=True, timeout=120)
    (d / (name + '.stdout')).write_text(z.stdout)
    (d / (name + '.stderr')).write_text(z.stderr)
    if z.returncode:
        raise RuntimeError(f'{name}: {z.returncode}; see {d}')
    return z
run(base + extra + ['-c', str(r / 'glx_nir_capture.c'), '-o', str(d / 'capture.o')], 'capture-compile')
link = next(x for x in commands if '-shared' in x)
link_inputs = [Path(x) for x in link if x.endswith(('.o', '.a')) and Path(x).is_file()]
link_hashes = {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in link_inputs}
run(link[:-2] + extra + ['-Wl,--wrap=remesa_shader_ir_lower', str(d / 'capture.o'),
    '-o', str(d / 'libGL-remesa-lab.so.1')], 'capture-link')
assert link_hashes == {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in link_inputs}
(d / 'LINK_INPUT_SHA256.json').write_text(json.dumps(link_hashes, indent=2) + '\n')
# Reuse verified dependency search paths, without relinking the software winsys.
search = [x for x in link if x.startswith('-L')]
paths = [x[2:] for x in search]
run(base + extra + [str(r / 'test_glx_fixed_function_gap.c'), '-L' + str(d),
    '-l:libGL-remesa-lab.so.1'] + search + ['-lX11', '-lxcb'] +
    ['-Wl,-rpath-link,' + x for x in paths] + ['-Wl,-rpath,$ORIGIN',
    '-o', str(d / 'probe')], 'probe-compile')
# The pinned X11 dependency extraction has the versioned file but no runtime
# SONAME link. Keep that alias private rather than changing its source tree.
xext = [f for path in paths for f in Path(path).glob('libXext.so.6.*') if f.is_file()]
if len(xext) != 1:
    raise RuntimeError('Expected exactly one pinned Xext runtime')
(d / 'libXext.so.6').symlink_to(xext[0].resolve())
run_env = dict(env, DISPLAY=a.display,
    LD_LIBRARY_PATH=str(d) + ':' + str(a.runtime_lib.resolve()) + ':' + str(c / 'runtime/lib'))
results = []
for mode in ('0', '1'):
    z = run([str(a.qemu), '-L', str(c / 'runtime'), str(d / 'probe'), mode],
            'unlit' if mode == '0' else 'lit', run_env)
    assert 'original_glxgears_depth_visual=REJECTED' in z.stdout
    assert 'submit_calls=0 published=0' in z.stdout
    assert 'reMESA recording backend (NO SGX)' in z.stdout
    captures = re.findall(r'REMESA_NIR_CAPTURE_BEGIN stage=(\d+).*?\n(.*?)REMESA_NIR_CAPTURE_END stage=\1 error=(\d+)', z.stderr, re.S)
    assert captures and all(int(error) == 95 for _, _, error in captures)
    results.append({'lighting': mode == '1', 'captures': [
        {'stage': int(stage), 'lower_error': int(error),
         'alu_histogram': {op: len(re.findall(r'= ' + op + r'\b', text))
                          for op in ('fmul', 'fadd', 'fdot3', 'frsq', 'fmax', 'slt')},
         'shader': text} for stage, text, error in captures], 'GPU': False})
inputs = [r / 'glx_nir_capture.c', r / 'test_glx_fixed_function_gap.c', r / 'test_glx_lab_window.c', Path(__file__).resolve()]
(d / 'SOURCE_SHA256.json').write_text(json.dumps({str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}, indent=2) + '\n')
(d / 'TEST_RESULTS.json').write_text(json.dumps({'status': 'CPU_DIAGNOSTIC_REPRODUCTION_PASS',
    'scope': 'Synthetic valid GL fixed-function sequence, not original glxgears; no GPU execution',
    'ubsan_probe_and_wrapper': a.ubsan, 'prebuilt_Mesa_instrumented': False,
    'cases': results, 'submissions': 0}, indent=2) + '\n')
print('CPU_DIAGNOSTIC_REPRODUCTION_PASS: depth visual rejected; 2 actual-Mesa shader paths captured; GPU=NO')
