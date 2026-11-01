#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Actual closed scene controller, modeled dependencies. No device opened."""
import argparse,hashlib,json,re,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--module',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path);a=p.parse_args()
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
rows=[];variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
if a.i486_cc:variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
for label,cc,flags,runner in variants:
 exe=o/label;cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*flags,'-I'+str(o),'-I'+str(m),'-I'+str(w/'kernel/remesa/gma500'),'-I'+str(w/'include'),str(w/'kernel/remesa/gma500/tests/test_scene_probe_handler.c'),'-o',str(exe)]
 for name,argv in [(label+'-build',cmd)]+[(label+'-'+str(c),runner+[str(exe),str(c)]) for c in range(44)]:
  def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,((6*1024**3 if runner else 768*1024**2),)*2)
  q=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits);(o/(name+'.log')).write_text(q.stdout+q.stderr);rows.append({'label':name,'argv':argv,'exit':q.returncode});(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
  if q.returncode or q.stderr:raise RuntimeError(name+': '+q.stderr[-1800:])
files=[Path(__file__).resolve(),w/'kernel/remesa/gma500/tests/test_scene_probe_handler.c']+[w/'kernel/remesa/gma500'/n for n in ['remesa_scene_probe.inc','remesa_scene_probe_request.h','remesa_scene_probe_profiles.h']]+[w/'include/remesa/scene_probe_uapi.h',o/'scene_probe_types.h']
q={'status':'CLOSED_GENERAL_SCENE_ADMISSION_CONTROLLER_MODEL_PASS','GPU_operations':0,'cases':44,'variants':len(variants),'scope':'Actual controller/envelope/profile translation; memory/crypto/source/hardware execution and lifetime dependencies modeled','sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'commands':rows};(o/'RESULT.json').write_text(json.dumps(q,indent=2)+'\n');print(q['status'])
