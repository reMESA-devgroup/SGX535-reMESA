#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the actual probe controller with modeled dependencies; never opens DRM.
Requires a staged private module source directory for its exact profile/structs.
Use --runner PROGRAM ARG ... to run a cross-compiled executable (appended last).
"""
import argparse
from pathlib import Path
import hashlib
import json
import re
import resource
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--module', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--cc', default='cc')
p.add_argument('--ubsan', action='store_true')
p.add_argument('--runner', nargs='+', default=[])
a=p.parse_args()
w=Path(__file__).resolve().parents[2];r=a.output.resolve();m=a.module.resolve()
r.mkdir(parents=True, exist_ok=False)
owner=(m/'gma500_bo_owner.h').read_text()
owner='\n'.join(re.findall(r'struct sgx535_gma500_(?:bo|owner) \{.*?\n\};',owner,re.S))
backend=re.search(r'struct sgx535_gma500_fixed_backend \{.*?\n\};',(m/'gma500_fixed_backend.h').read_text(),re.S)[0]
prefix="""#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
typedef uint32_t u32;typedef uint64_t u64;typedef uint8_t u8;
#include "frozen_kernel_contract.h"
#include "frozen_fixed_service.h"
struct mutex {bool held;};
struct page {unsigned long pfn;};
struct resource {u64 start,end;};
struct drm_gem_object {u64 size;};
struct drm_psb_private {void *sgx_reg;};
struct drm_device {struct drm_psb_private *dev_private;};
struct drm_file {unsigned value;};
#include "remesa_gem_memory.h"
"""
suffix="""
struct remesa_stream_observation {unsigned state;};
struct backing {bool live;u32 generation;struct resource va;struct drm_gem_object gem;};
struct retained_job {bool live;u64 cookie;struct remesa_memory_ticket ticket;};
struct remesa_gem_memory {
 struct drm_device *dev;struct drm_file *file;u64 context;struct resource root;
 struct mutex lock;void *executor;const void *private_session;bool stream_bound,ta_cache_domains;
 struct retained_job jobs[8];struct backing bo[32];struct page page;
};
"""
(r/'probe_handler_types.h').write_text(prefix+owner+'\n'+backend+suffix)
rows=[]
def limit(ceiling):
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    resource.setrlimit(resource.RLIMIT_AS,(ceiling,ceiling))
def run(label,argv,ceiling):
    result=subprocess.run(argv,capture_output=True,text=True,timeout=90,
                          preexec_fn=lambda:limit(ceiling))
    (r/(label+'.stdout')).write_text(result.stdout)
    (r/(label+'.stderr')).write_text(result.stderr)
    rows.append(dict(label=label,argv=argv,exit=result.returncode))
    (r/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
    if result.returncode or result.stderr:
        raise RuntimeError(label+': '+result.stderr[-4000:])
    return result
exe=r/'test-controller'
source=w/'kernel/remesa/gma500/tests/test_private_probe_handler.c'
flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if a.ubsan else []
run('build',[a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
     '-DREMESA_LAB_DEPTH1280_PROFILE=1','-I'+str(r),'-I'+str(m),'-I'+str(w/'include'),
     str(source),*flags,'-o',str(exe)],768*1024**2)
for case in range(37):
    result=run('case-'+str(case),[*a.runner,str(exe),str(case)],
               6*1024**3 if a.runner else 768*1024**2)
    if not result.stdout.startswith('PASS '):raise RuntimeError('missing test result')
inputs=[source,w/'kernel/remesa/gma500/remesa_private_probe.inc',
        m/'gma500_bo_owner.h',m/'gma500_fixed_backend.h',m/'stream_profile_asset.h',
        r/'probe_handler_types.h']
report=dict(status='ACTUAL_PROBE_CONTROLLER_MODEL_PASS',GPU_operations=0,cases=37,
            dependencies='GEM/crypto/grammar/borrower/source/private execution modeled',
            sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs})
(r/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
