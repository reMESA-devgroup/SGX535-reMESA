#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned Mesa registration + mock XCB publication. No GPU or X server access."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ['mesa','generated','overlay','output']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--i386-cc',type=Path);p.add_argument('--i386-sysroot',type=Path);p.add_argument('--qemu-i386',type=Path)
a=p.parse_args();here=Path(__file__).resolve().parents[1];root=here.parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
pin=json.loads((here/'mesa-api-pin.json').read_text())
for rel,h in pin['interface_files'].items():
 if sha(a.mesa/rel)!=h:raise SystemExit('Mesa header mismatch: '+rel)
if sha(a.generated/'util/format/u_format_gen.h')!=pin['generated_formats_sha256']:raise SystemExit('Generated formats mismatch')
manifest=json.loads((a.overlay/'integration.json').read_text())
for rel,h in manifest['files'].items():
 if sha(a.overlay/rel)!=h:raise SystemExit('Overlay mismatch: '+rel)
s=(a.overlay/'src/gallium/auxiliary/pipe-loader/pipe_loader_drm.c').read_text()
begin=s.index('static const struct drm_driver_descriptor *driver_descriptors[] = {')
end=s.index('\nstatic int\nget_nctx_caps',begin)
selection=s[begin:end];names=re.findall(r'&([a-z0-9_]+_driver_descriptor)',selection)
if names.count('gma500_driver_descriptor')!=1:raise SystemExit('Descriptor not registered once')
a.output.mkdir(parents=True,exist_ok=False)
# Compile the actual pinned selector/registered table with inert other descriptors.
# This tests selection; it is explicitly not a full Mesa DRI megadriver build.
text='#include "remesa_dri.h"\n#include <string.h>\n'
for n in dict.fromkeys(names+['kmsro_driver_descriptor']):
 if n!='gma500_driver_descriptor':text+='static const struct drm_driver_descriptor '+n+'={.driver_name="'+n.removesuffix('_driver_descriptor')+'"};\n'
text+='#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wsign-compare"\n'+selection+'\n#pragma GCC diagnostic pop\n'+'\nconst struct drm_driver_descriptor *remesa_test_find_driver(const char *name) {return get_driver_descriptor(name);}\n'
selector=a.output/'registered-selector.c';selector.write_text(text)
# Architecture-neutral licensed XCB declarations. No host library needed by mocks.
headers=a.output/'headers/xcb';headers.mkdir(parents=True)
for name in ['xcb.h','xproto.h']:shutil.copyfile('/usr/include/xcb/'+name,headers/name)
base=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DHAVE_STRUCT_TIMESPEC','-DHAVE_PTHREAD','-std=c11','-DREMESA_WITH_MESA_HEADERS','-D_POSIX_C_SOURCE=200809L','-D_DEFAULT_SOURCE','-DHAVE_ENDIAN_H','-Wall','-Wextra','-Werror',
 '-I'+str(root/'include'),'-I'+str(here),'-isystem'+str(a.generated/'compiler'),'-isystem'+str(a.generated/'compiler/nir'),'-isystem'+str(a.generated),'-I'+str(a.output/'headers'),'-I'+str(a.mesa/'src/gallium/include'),'-isystem'+str(a.mesa/'src/compiler/nir'),'-isystem'+str(a.mesa/'src'),'-I'+str(a.mesa/'include')]
variants=[('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
if any([a.i386_cc,a.i386_sysroot,a.qemu_i386]):
 if not all([a.i386_cc,a.i386_sysroot,a.qemu_i386]):raise SystemExit('Incomplete i386 triplet')
 variants.append(('i386',[str(a.i386_cc),'--sysroot='+str(a.i386_sysroot)],['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
env=dict(os.environ)
if a.i386_sysroot:env['LD_LIBRARY_PATH']=str(a.i386_sysroot/'usr/lib/aarch64-linux-gnu')
result={'scope':'Actual pinned Mesa descriptor selector with mocked identities; mock XCB checked requests; no OpenGL/DRI shared-library or physical acceleration claim','variants':{}}
for variant,cc,flags,runner in variants:
 result['variants'][variant]={}
 for target in ['dri','xcb','present_scale']:
  sources=([here/n for n in ['remesa_fixed_state.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_screen.c','remesa_draw.c','remesa_retained_transport.c','remesa_linux.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_usse_fragment_color.c','remesa_usse_arithmetic.c','remesa_program_registers.c','remesa_nir.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_loader.c','remesa_dri.c','tests/test_dri.c']]+[root/'common/remesa/draw_packet.c',root/'common/remesa/retained_evidence.c',root/'common/remesa/shader_request.c',selector] if target=='dri' else [here/n for n in ['remesa_present.c','remesa_present_scale.c','remesa_xcb.c','tests/test_'+target+'.c']])
  if target=='dri':sources += [here/n for n in ['remesa_persistent.c','remesa_linux_memory.c']]+[root/'kernel/remesa'/n for n in ['registry.c','resource.c','provider.c']]
  command=cc+base+flags+[str(s) for s in sources]+(['-Wl,--wrap=open','-Wl,--wrap=fstat','-Wl,--wrap=ioctl'] if target=='dri' else [])+['-o',str(a.output/(variant+'-'+target))]
  built=subprocess.run(command,capture_output=True,text=True,env=env);(a.output/(variant+'-'+target+'-compile.log')).write_text(built.stdout+built.stderr)
  if built.returncode:raise SystemExit('Compile failed: '+variant+' '+target)
  run=subprocess.run(runner+[str(a.output/(variant+'-'+target))],capture_output=True,text=True,env=env);(a.output/(variant+'-'+target+'-run.log')).write_text(run.stdout+run.stderr)
  if run.returncode:raise SystemExit('Test failed: '+variant+' '+target)
  result['variants'][variant][target]={'PASS':True,'command':command,'output':run.stdout.strip()}
result['xcb_header_hashes']={p.name:sha(p) for p in headers.iterdir()}
result['overlay_manifest_sha256']=sha(a.overlay/'integration.json')
(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({v:{t:r['PASS'] for t,r in ts.items()} for v,ts in result['variants'].items()}))
