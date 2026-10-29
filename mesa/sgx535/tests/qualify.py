#!/usr/bin/env python3
"""Compile native callbacks and synthetic provider tests against pinned Mesa.
No SGX backend or hardware execution is linked.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--mesa', type=Path, required=True)
parser.add_argument('--generated', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--targets', help='Comma-separated focused targets; default runs all')
parser.add_argument('--i386-cc', type=Path)
parser.add_argument('--i386-sysroot', type=Path)
parser.add_argument('--qemu-i386', type=Path)
a = parser.parse_args()
here = Path(__file__).resolve().parents[1]
pin = json.loads((here/'mesa-api-pin.json').read_text())
for name, expected in pin['interface_files'].items():
    actual = hashlib.sha256((a.mesa/name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit('Mesa interface mismatch: '+name)
if hashlib.sha256((a.generated/'util/format/u_format_gen.h').read_bytes()).hexdigest() != pin['generated_formats_sha256']:
    raise SystemExit('Mesa generated format header mismatch')
a.output.mkdir(parents=True, exist_ok=True)
base = ['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DHAVE_PTHREAD','-DHAVE_SECURE_GETENV','-DHAVE_STRUCT_TIMESPEC','-DREMESA_WITH_MESA_HEADERS', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-DHAVE_ENDIAN_H',
        '-Wall', '-Wextra', '-Werror', '-I'+str(a.generated),'-isystem'+str(a.generated/'compiler'),'-isystem'+str(a.generated/'compiler/nir'),'-isystem'+str(a.mesa/'src/compiler/nir'),
        '-I'+str(a.mesa/'src/gallium/include'), '-I'+str(a.mesa/'src'),
        '-I'+str(a.mesa/'include'), '-I'+str(here), '-I'+str(here.parents[1]/'include')]
sources = [str(here/name) for name in ['remesa_fixed_state.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_screen.c','remesa_draw.c','remesa_present.c']] + [str(here.parents[1]/'common/remesa/draw_packet.c')]
variants = [('native',['cc'],['-O2'],[]),
            ('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
if any([a.i386_cc,a.i386_sysroot,a.qemu_i386]):
    if not all([a.i386_cc,a.i386_sysroot,a.qemu_i386]):
        raise SystemExit('i386 compiler/sysroot/emulator must be supplied together')
    variants.append(('i386',[str(a.i386_cc),'--sysroot='+str(a.i386_sysroot)],
                     ['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
env = dict(os.environ)
if a.i386_sysroot:
    env['LD_LIBRARY_PATH'] = str(a.i386_sysroot/'usr/lib/aarch64-linux-gnu')
targets=[('copy_clear','test_copy_clear.c'),('clip_prepare','test_clip_prepare.c'), ('prepare_draw','test_prepare_draw.c'), ('transfers','test_transfer_resources.c'), ('framebuffer', 'test_framebuffer_contract.c'), ('context_contracts', 'test_context_contracts.c'), ('screen_caps', 'test_screen_caps.c'), ('clear', 'test_cpu_clear.c'), ('primary', 'test_draw.c'), ('review', 'test_draw_review.c'), ('driver', 'test_driver.c'), ('packing', 'test_pack.c'), ('resource_sync', 'test_resource_sync.c'), ('persistent', 'test_persistent.c'), ('integration', 'test_provider_integration.c')]
if a.targets:
    chosen=a.targets.split(',')
    if len(set(chosen))!=len(chosen) or any(name not in dict(targets) for name in chosen):
        raise SystemExit('Unknown or duplicate target')
    targets=[item for item in targets if item[0] in chosen]
results = {}
for name, compiler, flags, prefix in variants:
    results[name] = {}
    for target, source in targets:
        exe=a.output/(name+'-'+target)
        extra = [str(here.parents[1]/('kernel/remesa/'+name)) for name in ['registry.c','resource.c','provider.c']] if target in ('integration','persistent') else []
        if target=='persistent':
            extra += [str(here/n) for n in ['remesa_persistent.c','remesa_shader_program.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_program_registers.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_nir.c']]
            extra.append(str(here.parents[1]/'common/remesa/shader_request.c'))
        # Screen-wide 64-bit cookie atomics require libatomic on i486.
        command=compiler+base+flags+sources+extra+[str(here/'tests'/source),'-o',str(exe),'-latomic']
        built=subprocess.run(command, capture_output=True, text=True, env=env)
        (a.output/(name+'-'+target+'-compile.log')).write_text(built.stdout+built.stderr)
        if built.returncode: raise SystemExit('Compile failed: '+name+' '+target)
        run=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,env=env)
        (a.output/(name+'-'+target+'-run.log')).write_text(run.stdout+run.stderr)
        if run.returncode: raise SystemExit('Qualification failed: '+name+' '+target)
        results[name][target]={'PASS':True,'command':command,'output':run.stdout.strip()}
(a.output/'result.json').write_text(json.dumps({'mesa_pin':pin['version'],
    'scope':'Gallium callbacks and synthetic clean-provider integration; no SGX execution','variants':results},indent=2)+'\n')
print(json.dumps({name: {target: test['output'] for target,test in result.items()} for name,result in results.items()},indent=2))
