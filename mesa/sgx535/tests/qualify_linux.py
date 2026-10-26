#!/usr/bin/env python3
"""Build the Gallium test; qualify interposed transport without hardware.
The hardware application is never executed by this offline qualification.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--mesa',type=Path,required=True);p.add_argument('--generated',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--target',action='append',choices=['application','transport','integration','loader','shader'],help='Limit focused qualification to selected targets')
p.add_argument('--i386-cc',type=Path);p.add_argument('--i386-sysroot',type=Path);p.add_argument('--qemu-i386',type=Path)
a=p.parse_args();here=Path(__file__).resolve().parents[1];root=here.parents[1]
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
pin=json.loads((here/'mesa-api-pin.json').read_text())
for name,value in pin['interface_files'].items():
 if sha(a.mesa/name)!=value:raise SystemExit('Mesa pin mismatch: '+name)
if sha(a.generated/'util/format/u_format_gen.h')!=pin['generated_formats_sha256']:raise SystemExit('Format header mismatch')
base=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DHAVE_STRUCT_TIMESPEC','-DHAVE_PTHREAD','-DREMESA_WITH_MESA_HEADERS','-std=c11','-D_POSIX_C_SOURCE=200809L','-DHAVE_ENDIAN_H','-Wall','-Wextra','-Werror',
 '-I'+str(root/'include'),'-I'+str(here),'-isystem'+str(a.generated/'compiler'),'-isystem'+str(a.generated/'compiler/nir'),'-isystem'+str(a.generated),'-isystem'+str(a.mesa/'src/gallium/include'),'-isystem'+str(a.mesa/'src/compiler/nir'),'-isystem'+str(a.mesa/'src'),'-isystem'+str(a.mesa/'include')]
variants=[('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
if any([a.i386_cc,a.i386_sysroot,a.qemu_i386]):
 if not all([a.i386_cc,a.i386_sysroot,a.qemu_i386]):raise SystemExit('i386 triplet incomplete')
 variants.append(('i386',[str(a.i386_cc),'--sysroot='+str(a.i386_sysroot)],['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
a.output.mkdir(parents=True,exist_ok=True);env=dict(os.environ)
if a.i386_sysroot:env['LD_LIBRARY_PATH']=str(a.i386_sysroot/'usr/lib/aarch64-linux-gnu')
result={'scope':'semantic marshalling and interposed transport tested; applications compiled NOT executed; no SGX','variants':{}}
inputs=[here/'remesa_fixed_state.c',here/'remesa_render_pass.c',here/'remesa_sampler_state.c',here/'remesa_texture_binding.c',here/'remesa_texture_descriptor.c',here/'remesa_screen.c',here/'remesa_draw.c',here/'remesa_linux.c',root/'common/remesa/draw_packet.c',root/'common/remesa/retained_evidence.c',root/'common/remesa/shader_request.c',
 here/'tests/gallium_first_draw.c',here/'tests/test_linux_transport.c',here/'tests/test_gallium_linux_integration.c',root/'include/remesa/native_uapi.h',
 root/'include/remesa/draw_packet.h',here/'remesa_linux.h',here/'remesa_device.h',here/'remesa_screen.h',here/'remesa_draw.h',here/'mesa-api-pin.json']
shader_sources=[here/n for n in ['remesa_retained_transport.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_usse_fragment_color.c','remesa_usse_arithmetic.c','remesa_program_registers.c','remesa_nir.c','remesa_fragment_body.c','remesa_shader_storage.c']]
shader_sources += [here/n for n in ['remesa_persistent.c','remesa_linux_memory.c']]+[root/'kernel/remesa'/n for n in ['registry.c','resource.c','provider.c']]
inputs += shader_sources+[root/'include/remesa/shader_uapi.h',root/'include/remesa/shader_request.h',here/'remesa_fragment_body.h',here/'remesa_shader_storage.h',here/'tests/test_native_shader.c']
inputs += [here/'remesa_loader.c',here/'remesa_loader.h',here/'tests/test_loader.c']
for name,cc,flags,emu in variants:
 result['variants'][name]={}
 for target in a.target or ['application','transport','integration','loader','shader']:
  sources=([here/'remesa_fixed_state.c',here/'remesa_render_pass.c',here/'remesa_sampler_state.c',here/'remesa_texture_binding.c',here/'remesa_texture_descriptor.c',here/'remesa_screen.c',here/'remesa_draw.c'] if target in ('application','integration','loader') else [])+[here/'remesa_linux.c',root/'common/remesa/draw_packet.c',root/'common/remesa/retained_evidence.c',root/'common/remesa/shader_request.c',here/'tests'/('gallium_first_draw.c' if target=='application' else 'test_gallium_linux_integration.c' if target=='integration' else 'test_loader.c' if target=='loader' else 'test_native_shader.c' if target=='shader' else 'test_linux_transport.c')]+shader_sources
  if target=='loader':sources.append(here/'remesa_loader.c')
  # Native shader-token destruction now owns the raw draw snapshot too.
  if here/'remesa_draw.c' not in sources:sources.append(here/'remesa_draw.c')
  exe=a.output/(name+'-'+target);command=cc+base+flags+(['-Wl,--wrap=open','-Wl,--wrap=close','-Wl,--wrap=ioctl'] if target=='integration' else ['-Wl,--wrap=ioctl'] if target=='loader' else ['-Wl,--wrap=calloc','-Wl,--wrap=malloc','-Wl,--wrap=free'] if target=='shader' else [])+[str(x) for x in sources]+['-o',str(exe)]
  r=subprocess.run(command,capture_output=True,text=True,env=env);(a.output/(name+'-'+target+'-compile.log')).write_text(r.stdout+r.stderr)
  if r.returncode:raise SystemExit('Compile failed: '+name+' '+target)
  output=None
  if target!='application':
   r=subprocess.run(emu+[str(exe)],capture_output=True,text=True,env=env);(a.output/(name+'-'+target+'-run.log')).write_text(r.stdout+r.stderr)
   if r.returncode:raise SystemExit('Transport regression failed: '+name)
   output=r.stdout.strip()
  result['variants'][name][target]={'PASS':True,'executed':target!='application','command':command,'output':output,'bytes':exe.stat().st_size,'sha256':sha(exe)}
result['source_sha256']={str(x.relative_to(root)):sha(x) for x in inputs}
(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({name:{t:rec['PASS'] for t,rec in value.items()} for name,value in result['variants'].items()},indent=2))
