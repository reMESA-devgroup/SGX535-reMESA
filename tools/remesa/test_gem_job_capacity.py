#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Retained core + private loan and real MMU auditor models; no device access."""
import argparse,hashlib,json,os,resource,subprocess
from pathlib import Path
from stage_twod_reservation import PINNED,job_capacity_sources

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--native-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path)
 a=p.parse_args();w=Path(__file__).resolve().parents[2];n=a.native_source.resolve();o=a.output.resolve()
 if o.exists():raise FileExistsError(o)
 if o==n or o.is_relative_to(n):raise ValueError('output must be outside frozen native input')
 sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
 for name in ['remesa_gem_memory.c','remesa_gem_memory.h']:
  if sha(n/name)!=PINNED[name]:raise ValueError('native hash mismatch: '+name)
 original=(n/'remesa_gem_memory.c').read_text();header=(n/'remesa_gem_memory.h').read_text()
 core,expanded=job_capacity_sources(original,header)
 # Reject absent/duplicate anchors instead of silently widening the wrong source.
 for old in ['cpu_views[16]','ids[16]','count>16','uses[16]','added[16]']:
  for bad in [original.replace(old,old.replace('16','15'),1),original+'\n/* '+old+' */\n']:
   try:job_capacity_sources(bad,header)
   except ValueError:pass
   else:raise AssertionError('ambiguous/missing anchor accepted: '+old)
 o.mkdir(parents=True);(o/'remesa_gem_memory.c').write_text(core);(o/'remesa_gem_memory.h').write_text(expanded)
 shim=(n.parent/'tests/test_gem_shim.h').read_text();anchor='struct psb_mmu_pd {int hw_context;};'
 if shim.count(anchor)!=1:raise ValueError('shim driver anchor mismatch')
 (o/'test_gem_shim.h').write_text(shim.replace(anchor,'struct psb_mmu_pd {int hw_context;struct psb_mmu_driver *driver;};'))
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc or a.qemu_i386:
  if not(a.i486_cc and a.qemu_i386):p.error('both i486 options required')
  variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 rows=[];files=[Path(__file__).resolve(),w/'tools/remesa/stage_twod_reservation.py',n/'remesa_gem_memory.c',n/'remesa_gem_memory.h',n.parent/'tests/test_gem_shim.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_job_limits.h','remesa_gem_private.h','remesa_gem_private.inc','remesa_mmu_confinement.h','remesa_mmu_confinement.inc']]
 files += [w/'kernel/remesa/gma500/tests'/name for name in ['test_gem_job_capacity.c','test_gem_private.c','test_gem_twod.c','test_mmu_job_capacity.c','test_mmu_confinement.c']]
 before={str(f):sha(f) for f in files};env={k:v for k,v in os.environ.items() if k not in ('LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH')}
 def run(label,cmd,ceiling):
  def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(ceiling,ceiling))
  q=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120,preexec_fn=limit)
  (o/(label+'.log')).write_text(q.stdout+q.stderr);rows.append(dict(label=label,argv=cmd,exit=q.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
  if q.returncode or q.stderr:raise RuntimeError(label+': '+q.stderr[-2000:])
  return q.stdout
 includes=['-I'+str(o),'-I'+str(n),'-I'+str(w/'include'),'-I'+str(w/'kernel/remesa/gma500'),'-I'+str(w/'mesa/sgx535')]
 for variant,cc,flags,runner in variants:
  for name in ['test_gem_job_capacity','test_mmu_job_capacity']:
   exe=o/(variant+'-'+name);test=w/'kernel/remesa/gma500/tests'/(name+'.c')
   sources=[str(test)]+([str(w/'common/remesa/draw_packet.c'),str(w/'common/remesa/shader_request.c')] if name=='test_gem_job_capacity' else [])
   command=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers',*flags,*includes,*sources,'-o',str(exe)]
   run(variant+'-'+name+'-build',command,768*1024**2)
   out=run(variant+'-'+name+'-run',runner+[str(exe)],6*1024**3 if runner else 768*1024**2);print(out.strip(),flush=True)
 if any(sha(Path(f))!=h for f,h in before.items()):raise RuntimeError('source changed during qualification')
 (o/'RESULT.json').write_text(json.dumps(dict(status='RETAINED_JOB_AND_MMU_CAPACITY32_MODELS_PASS',GPU_operations=0,scope='Real transformed retained core/private loan/MMU auditor with modeled kernel primitives; no native executor/GPU qualification',sources=before,generated={str(f):sha(f) for f in [o/'remesa_gem_memory.c',o/'remesa_gem_memory.h',o/'test_gem_shim.h']},commands=rows),indent=2)+'\n')
if __name__=='__main__':main()
