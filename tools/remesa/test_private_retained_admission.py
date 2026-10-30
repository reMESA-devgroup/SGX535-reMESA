#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual retained ioctl admission; downstream callbacks modeled, GPU0."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from test_private_source_mmio import extract

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--module',type=Path,required=True);p.add_argument('--baseline-module',type=Path,required=True)
 p.add_argument('--profile-include',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path)
 a=p.parse_args();w=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc or a.qemu_i386:
  if not (a.i486_cc and a.qemu_i386):p.error('both i486 options required')
  variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 # Only ioctl-number macros are shimmed; the request fields/lengths come from
 # the exact frozen profile header used by the existing physical module build.
 (o/'drm').mkdir();(o/'drm/drm.h').write_text('#define DRM_COMMAND_BASE 0x40\n#define DRM_IOWR(n,t) 0\n')
 rows=[];test=w/'kernel/remesa/gma500/tests/test_private_retained_admission.c'
 for kind,module in [('baseline',a.baseline_module),('candidate',a.module)]:
  d=o/kind;d.mkdir();(d/'retained_ioctl_actual.inc').write_text(extract((module/'remesa_gem_memory.c').read_text(),'remesa_retained_submit_ioctl'))
  for name,cc,flags,runner in variants:
   if kind=='baseline' and name!='native':continue
   exe=d/name;mode=['-DPRIVATE_ADMISSION_OLD_WITNESS'] if kind=='baseline' else []
   command=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,*mode,'-I'+str(d),'-I'+str(o),'-I'+str(a.profile_include.resolve()),str(test),'-o',str(exe)]
   for label,argv in [(kind+'-'+name+'-build',command),(kind+'-'+name+'-run',runner+[str(exe)])]:
    def limit():
     n=6*1024**3 if runner and label.endswith('-run') else 768*1024**2
     resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(n,n))
    r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limit);(o/(label+'.log')).write_text(r.stdout+r.stderr)
    rows.append(dict(label=label,argv=argv,exit=r.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
    if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
 files=[test,Path(__file__).resolve(),a.module/'remesa_gem_memory.c',a.baseline_module/'remesa_gem_memory.c',a.profile_include/'remesa/retained_uapi.h']
 report=dict(status='ACTUAL_RETAINED_PRIVATE_LEASE_ADMISSION_PASS',GPU_operations=0,scope='Actual ioctl and frozen request envelope; downstream init/capture/submission/locks modeled; no physical execution or ABI-portability claim',sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
 (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'variants':len(variants)}))
if __name__=='__main__':main()
