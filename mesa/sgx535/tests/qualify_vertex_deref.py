#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise actual pinned Mesa NIR structs and callbacks; no hardware."""
import json,os,subprocess,argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--test',choices=['test_vertex_deref_nir.c'],default='test_vertex_deref_nir.c');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];m=a.cache;s=m/'mesa-25.2.0';b=m/'build-i386';a.output.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env.update(json.loads((m/'build-environment.json').read_text()))
base=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-std=c11','-D_GNU_SOURCE','-DHAVE_ENDIAN_H','-DHAVE_SECURE_GETENV','-DHAVE_PTHREAD','-DHAVE_STRUCT_TIMESPEC','-DREMESA_WITH_MESA_HEADERS','-DREMESA_WITH_NIR']
base+=['-I'+str(x) for x in [repo/'include',repo/'mesa/sgx535',s/'include',s/'src',s/'src/gallium/include',s/'src/gallium/auxiliary',s/'src/compiler/nir',b/'src/compiler/nir',b/'src/compiler',b/'src']]
sources=[repo/'mesa/sgx535'/x for x in ['remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_usse_fragment_color.c','remesa_usse_arithmetic.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c','remesa_fixed_state.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_screen.c','remesa_draw.c']]+[repo/'common/remesa/draw_packet.c',b/'src/compiler/nir/nir_intrinsics.c',s/'src/util/ralloc.c',s/'src/util/u_printf.c',s/'src/gallium/auxiliary/util/u_upload_mgr.c']
q='/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
results={}
for name,cc,flags,run in [('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(m/'wrappers/cc')],['-O2','-march=i486','-static'],[q])]:
 exe=a.output/name;cmd=cc+base+flags+list(map(str,sources))+[str(repo/'mesa/sgx535/tests'/a.test),'-o',str(exe)]
 r=subprocess.run(cmd,env=env,text=True,capture_output=True);(a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' compilation failed')
 r=subprocess.run(run+[str(exe)],env=env,text=True,capture_output=True);(a.output/(name+'-run.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' execution failed')
 results[name]={'pass':True,'command':cmd,'output':r.stdout.strip()}
(a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
