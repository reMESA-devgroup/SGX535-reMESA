#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual Mesa CSO cleanup against restricted driver; no GPU or GL context."""
import argparse,json,os,shlex,subprocess,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
repo=Path(__file__).resolve().parents[2];b=a.cache/'build-i386';env=dict(os.environ);env.update(json.loads((a.cache/'build-environment.json').read_text()))
r=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/remesa_nir.c'));args=r.get('arguments') or shlex.split(r['command']);flags=[];skip=False
for x in args:
 if skip:skip=False;continue
 if x in ('-o','-MF','-MQ'):skip=True;continue
 if x in ('-c','-MD','-DNDEBUG') or x==r['file']:continue
 flags.append(x)
flags.insert(1,'-I'+str(repo/'mesa/sgx535'));flags+=['-UNDEBUG','-DREMESA_TEST_REAL_CSO','-DREMESA_WITH_NIR','-DREMESA_WITH_MESA_HEADERS']
sources=[repo/'mesa/sgx535'/n for n in ['remesa_screen.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_fixed_state.c','remesa_draw.c','remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c','tests/test_context_contracts.c']]+[repo/'common/remesa/draw_packet.c',repo/'common/remesa/shader_request.c']
libs=['src/gallium/auxiliary/libgallium.a','src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a','src/util/libmesa_util_simd.a','src/util/blake3/libblake3.a','src/util/format/libmesa_format.a','src/c11/impl/libmesa_util_c11.a']
exe=a.output/'cso-i386';cmd=flags+list(map(str,sources))+['-Wl,--start-group']+[str(b/n) for n in libs if (b/n).exists()]+['-Wl,--end-group','-L'+str(a.cache/'i386-deps/usr/lib/i386-linux-gnu'),'-ldrm','-lm','-ldl','-latomic','-pthread',str(a.cache/'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),'-o',str(exe)]
r=subprocess.run(cmd,cwd=b,env=env,capture_output=True,text=True);(a.output/'compile.log').write_text(r.stdout+r.stderr);(a.output/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
if r.returncode:raise SystemExit('CSO compile/link STOP')
q='/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';run=[q,'-L',str(a.cache/'runtime'),str(exe)];r=subprocess.run(run,env=env,capture_output=True,text=True);(a.output/'run.log').write_text(r.stdout+r.stderr)
(a.output/'result.json').write_text(json.dumps({'PASS':r.returncode==0,'run':run,'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'output':r.stdout+r.stderr,'scope':'Actual Mesa CSO, mocked packet/completion, no GL context or hardware'},indent=2)+'\n');print(r.stdout+r.stderr);raise SystemExit(r.returncode)
