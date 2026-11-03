#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Actual closed scene controller, modeled dependencies. No device opened."""
import argparse,hashlib,json,re,resource,subprocess,struct
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--module',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path);p.add_argument('--extended-captures',type=Path,help='Offline bound-export0/1 actual18-binding fixtures; no kernel catalogue installation');p.add_argument('--wire-v2',action='store_true',help='Exercise actual separately registered18-binding handlers');p.add_argument('--homogeneous',action='store_true',help='Explicit state15/W2 profiles3/4; requires --wire-v2');p.add_argument('--clamped-api',action='store_true',help='Actual API CODE344/state15 selectors5/6; requires --wire-v2');p.add_argument('--matrix-submit',action='store_true',help='Separate ordinary matrix controller; modeled source/execution dependencies');a=p.parse_args()
if a.homogeneous and (not a.wire_v2 or a.extended_captures):p.error('--homogeneous requires --wire-v2 without extended fixture override')
if a.clamped_api and (not a.wire_v2 or a.homogeneous or a.extended_captures):p.error('--clamped-api requires --wire-v2 without another fixture mode')
if a.matrix_submit and (a.wire_v2 or a.extended_captures or a.homogeneous or a.clamped_api):p.error('--matrix-submit is independent of LAB catalogue modes')
w=Path(__file__).resolve().parents[2];m=a.module.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);(o/'drm').mkdir();(o/'drm/drm.h').write_text('#define DRM_COMMAND_BASE 0x40\n#define DRM_IOWR(n,t) (n)\n')
backend=re.search(r'struct sgx535_gma500_fixed_backend \{.*?\n\};',(m/'gma500_fixed_backend.h').read_text(),re.S)[0]
prefix='''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint32_t u32;typedef uint64_t u64;typedef uint8_t u8;
struct mutex {bool held;};struct page {unsigned long pfn;};struct resource {u64 start,end;};
struct drm_gem_object {u64 size;};struct drm_psb_private {void *sgx_reg;};
struct drm_device {struct drm_psb_private *dev_private;};struct drm_file {unsigned value;};
#include "remesa_gem_scene_job.h"
#include "remesa_scene_packet.h"
#include "remesa_twod_reservation.h"
'''
suffix='''
struct remesa_stream_source_event {u32 kind,sequence,status1,status2,ledger,phase;};
struct remesa_stream_observation {unsigned state,event_count,zero_samples;struct remesa_stream_source_event events[64];u64 event_ns[64];};
struct backing {bool live,pinned,reserved,quarantine,lease,initialized,zero_pristine;unsigned holds,color_vmas;u32 generation;void *cached_job_view;struct resource va;struct drm_gem_object gem;};
struct retained_job {bool live;u64 cookie;};
struct remesa_gem_memory {struct drm_device *dev;struct drm_file *file;u64 context;struct resource root;struct mutex lock;const void *private_session;void *executor;bool stream_bound,ta_cache_domains,private_device_lost,private_source_held,private_source_poisoned;struct retained_job jobs[8];struct backing bo[32];};
'''
(o/'scene_probe_types.h').write_text(prefix+backend+suffix)
fixture_inputs=[]
if a.matrix_submit:
 (o/'remesa_matrix_submit_uapi.h').write_text((w/'include/remesa/matrix_submit_uapi.h').read_text().replace('"scene_packet.h"','"remesa_scene_packet.h"'))
 names=re.search(r'enum \{(.*?)\};',(w/'kernel/remesa/gma500/tests/test_scene_probe_handler.c').read_text(),re.S)[1].replace('\n','').split(',')
 excluded={'DUPALLOC','VA','SIZE','HASH','OK_INIT','OK640_INIT','OK_FINAL','OK640_FINAL','OK_GALLIUM','OK640_GALLIUM','VA_GALLIUM','HASH_GALLIUM','WRONG_LEGACY','OK_MATRIX','OK640_MATRIX','VA_MATRIX','HASH_MATRIX','WRONG_WINDOW'}
 matrix_cases=[i for i,name in enumerate(names) if name.strip() not in excluded]

if a.extended_captures:
 lines=[]
 for i,size in enumerate(['1280x800','640x640']):
  root=a.extended_captures/('bound-export-'+str(i))/size;packet=root/'packet.bin';placement=root/'placement.bin';raw=packet.read_bytes();where=placement.read_bytes()
  if len(raw)!=2264 or struct.unpack_from('<4I',raw,16)!=(1,18,11,2) or len(where)!=776 or struct.unpack_from('<II',where)!=(1,18):raise ValueError('not an actual closed18/11/2 fixture')
  fixture_inputs += [packet,placement]
  lines.append('static const u8 extended_packet'+str(i)+'[2264]={')
  lines += [','.join('0x%02x'%v for v in raw[j:j+24])+',' for j in range(0,len(raw),24)];lines.append('};')
  lines.append('static const struct remesa_scene_probe_binding extended_bindings'+str(i)+'[18]={')
  for j in range(18):
   va,n,rid,access=struct.unpack_from('<QQII',where,8+24*j)
   if struct.unpack_from('<II',raw,32+8*j)!=(rid,access) or access not in [1,3]:raise ValueError('placement mismatch')
   digest=bytes(32)
   if access==1:
    pimage=root/('bo-'+str(j)+'.bin');data=pimage.read_bytes();fixture_inputs.append(pimage)
    if len(data)!=n:raise ValueError('READ extent mismatch')
    digest=hashlib.sha256(data).digest()
   if j==17 and access!=1:raise ValueError('last-owner hash refusal fixture requires READ')
   lines.append('{0x%xULL,%du,%du,{%s}},'%(va,n,access,','.join('0x%02x'%v for v in digest)))
  lines.append('};')
 (o/'scene_probe_extended_fixture.h').write_text('\n'.join(lines)+'\n')
rows=[];variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
if a.i486_cc:variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
for label,cc,flags,runner in variants:
 exe=o/label;cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*flags,*(['-DREMESA_SCENE_CONTROLLER_EXTENDED'] if a.extended_captures else []),*(['-DREMESA_SCENE_CONTROLLER_WIRE_V2'] if a.wire_v2 else []),*(['-DREMESA_SCENE_CONTROLLER_HOMOGENEOUS'] if a.homogeneous else []),*(['-DREMESA_SCENE_CONTROLLER_CLAMPED_API'] if a.clamped_api else []),'-I'+str(o),'-I'+str(m),'-I'+str(w/'kernel/remesa/gma500'),'-I'+str(w/'include'),str(w/'kernel/remesa/gma500/tests'/('test_matrix_submit_handler.c' if a.matrix_submit else 'test_scene_probe_handler.c')),'-o',str(exe)]
 for name,argv in [(label+'-build',cmd)]+[(label+'-'+str(c),runner+[str(exe),str(c)]) for c in (matrix_cases if a.matrix_submit else range(44 if a.extended_captures or a.wire_v2 else 58))]:
  def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,((6*1024**3 if runner else 768*1024**2),)*2)
  q=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits);(o/(name+'.log')).write_text(q.stdout+q.stderr);rows.append({'label':name,'argv':argv,'exit':q.returncode});(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
  if q.returncode or q.stderr:raise RuntimeError(name+': '+q.stderr[-1800:])
files=fixture_inputs+([o/'scene_probe_extended_fixture.h'] if a.extended_captures else [])+[Path(__file__).resolve(),w/'kernel/remesa/gma500/tests/test_scene_probe_handler.c']+[w/'kernel/remesa/gma500'/n for n in ['remesa_scene_probe.inc','remesa_scene_probe_request.h','remesa_scene_probe_profiles.h','remesa_gears_scene_probe_profiles.h','remesa_gears_homogeneous_probe_profiles.h','remesa_gears_clamped_api_probe_profiles.h']]+[w/'include/remesa/scene_probe_uapi.h',w/'include/remesa/gears_scene_probe_uapi.h',w/'include/remesa/gears_scene_probe_envelope.h',o/'scene_probe_types.h']
if a.matrix_submit:
 files += [w/'kernel/remesa/gma500/tests/test_matrix_submit_handler.c',w/'kernel/remesa/gma500/remesa_matrix_submit.inc',w/'kernel/remesa/gma500/remesa_matrix_submit.h',w/'include/remesa/matrix_submit_uapi.h',o/'remesa_matrix_submit_uapi.h']
q={'status':'CLOSED_GENERAL_SCENE_ADMISSION_CONTROLLER_MODEL_PASS','GPU_operations':0,'cases':44 if a.extended_captures or a.wire_v2 else 58,'binding_capacity':18 if a.extended_captures or a.wire_v2 else 17,'public18_binding_wire_handler_exercised':a.wire_v2,'homogeneous_profiles':a.homogeneous,'clamped_api_profiles':a.clamped_api,'variants':len(variants),'scope':'Actual controller/envelope/profile translation; memory/crypto/source/hardware execution and lifetime dependencies modeled','sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'commands':rows};
if a.matrix_submit:q.update(status='ORDINARY_MATRIX_CONTROLLER_MODEL_PASS',cases=len(matrix_cases),binding_capacity=18,matrix_submit=True,case_names=[names[i].strip() for i in matrix_cases])
(o/'RESULT.json').write_text(json.dumps(q,indent=2)+'\n');print(q['status'])
