#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Retained core + private loan and real MMU auditor models; no device access."""
import argparse,hashlib,json,os,resource,subprocess
from pathlib import Path
from stage_twod_reservation import PINNED,job_capacity_sources,dpm_origin_source,replace_once,scene_action_components,scene_use_components
from test_private_source_mmio import extract

def indexed_dpm_shim(shim):
 # Model full64MiB without the old4096-leaf fixture ceiling or quadratic scans.
 shim=replace_once(shim,'test_pt[4096]','test_pt[1048576]')
 functions={
 'psb_mmu_insert_pages': r'''static int psb_mmu_insert_pages(struct psb_mmu_pd *pd,struct page **p,u64 va,unsigned n,int a,int b,int flags)
 {(void)pd;(void)a;(void)b;(void)flags;assert(n==1&&!(va&4095)&&va<0x100000000ULL);++inserts;
 if(fail_insert&&inserts==(unsigned)fail_insert)return -ENOMEM;
 unsigned i=(unsigned)(va>>12);if(test_pt[i].live)return -EBUSY;
 test_pt[i].va=va;test_pt[i].pfn=page_to_pfn(*p);test_pt[i].live=true;++ptes;return 0;}''',
 'psb_mmu_remove_pages': r'''static void psb_mmu_remove_pages(struct psb_mmu_pd *pd,u64 va,unsigned n,int a,int b)
 {(void)pd;(void)a;(void)b;assert(!(va&4095)&&ptes>=n);
 for(unsigned k=0;k<n;k++){unsigned i=(unsigned)(va>>12)+k;assert(i<1048576&&test_pt[i].live);test_pt[i].live=false;--ptes;}}''',
 'psb_mmu_virtual_to_pfn': r'''static int psb_mmu_virtual_to_pfn(struct psb_mmu_pd *pd,u32 va,unsigned long *pfn)
 {(void)pd;if(test_pfn_hook)return test_pfn_hook(va,pfn);unsigned i=va>>12;
 if(test_pt[i].live&&test_pt[i].va==va){*pfn=test_pt[i].pfn;return 0;}return -EINVAL;}''',
 'clear_highpage': r'''static void clear_highpage(struct page *p){test_zeroed_pages++;memset(p->bytes,0,PAGE_SIZE);}'''}
 shim=replace_once(shim,'static unsigned gems,pins,ptes,inserts,flushes,device_refs,module_refs;',
                   'static unsigned gems,pins,ptes,inserts,flushes,device_refs,module_refs,test_zeroed_pages;')
 for name,body in functions.items():shim=replace_once(shim,extract(shim,name),body)
 return shim

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--native-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--scene-actions',action='store_true',help='Test bound kernel scene action plans with17 real modeled allocations')
 p.add_argument('--dpm',action='store_true',help='Test cold DPM provenance with full64MiB and indexed model PTEs')
 p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path)
 a=p.parse_args()
 if a.dpm and a.scene_actions:p.error('select one focused model')
 w=Path(__file__).resolve().parents[2];n=a.native_source.resolve();o=a.output.resolve()
 if o.exists():raise FileExistsError(o)
 if o==n or o.is_relative_to(n):raise ValueError('output must be outside frozen native input')
 sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
 for name in ['remesa_gem_memory.c','remesa_gem_memory.h']:
  if sha(n/name)!=PINNED[name]:raise ValueError('native hash mismatch: '+name)
 original=(n/'remesa_gem_memory.c').read_text();header=(n/'remesa_gem_memory.h').read_text()
 core,expanded=job_capacity_sources(original,header)
 core=dpm_origin_source(core)+'\n#include "remesa_gem_dpm.inc"\n'
 if a.scene_actions:core+='\n#include "remesa_gem_scene_actions.inc"\n#include "remesa_gem_scene_controls.inc"\n#include "remesa_gem_scene_use.inc"\n#include "remesa_gem_scene_ta.inc"\n#include "remesa_gem_scene_states.inc"\n#include "remesa_gem_scene_vertices.inc"\n#include "remesa_gem_scene_fragments.inc"\n#include "remesa_gem_scene_background.inc"\n'
 # Reject absent/duplicate anchors instead of silently widening the wrong source.
 for old in ['cpu_views[16]','ids[16]','count>16','uses[16]','added[16]']:
  for bad in [original.replace(old,old.replace('16','15'),1),original+'\n/* '+old+' */\n']:
   try:job_capacity_sources(bad,header)
   except ValueError:pass
   else:raise AssertionError('ambiguous/missing anchor accepted: '+old)
 o.mkdir(parents=True);(o/'remesa_gem_memory.c').write_text(core);(o/'remesa_gem_memory.h').write_text(expanded)
 shim=(n.parent/'tests/test_gem_shim.h').read_text();anchor='struct psb_mmu_pd {int hw_context;};'
 if shim.count(anchor)!=1:raise ValueError('shim driver anchor mismatch')
 shim=shim.replace(anchor,'struct psb_mmu_pd {int hw_context;struct psb_mmu_driver *driver;};')
 if a.dpm or a.scene_actions:shim=indexed_dpm_shim(shim)
 (o/'test_gem_shim.h').write_text(shim)
 if a.scene_actions:
  for name,data in scene_action_components(w,n).items():(o/name).write_bytes(data)
  for name,data in scene_use_components(w).items():(o/name).write_bytes(data)
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc or a.qemu_i386:
  if not(a.i486_cc and a.qemu_i386):p.error('both i486 options required')
  variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 rows=[];files=[Path(__file__).resolve(),w/'tools/remesa/stage_twod_reservation.py',n/'remesa_gem_memory.c',n/'remesa_gem_memory.h',n.parent/'tests/test_gem_shim.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_actions.h','remesa_gem_scene_actions.inc','remesa_gem_dpm.h','remesa_gem_dpm.inc','remesa_gem_job_limits.h','remesa_gem_private.h','remesa_gem_private.inc','remesa_mmu_confinement.h','remesa_mmu_confinement.inc']]
 files += [w/'kernel/remesa/gma500/tests'/name for name in ['test_gem_scene_actions.c','test_gem_dpm.c','test_gem_job_capacity.c','test_gem_private.c','test_gem_twod.c','test_mmu_job_capacity.c','test_mmu_confinement.c']]
 files += [w/'tools/remesa/lab_scene_actions.c',w/'tools/remesa/lab_scene_actions.h',w/'include/remesa/scene_layout.h',w/'include/remesa/scene_dpm.h',w/'include/remesa/scene_control_words.h',n/'frozen_kernel_contract.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_controls.h','remesa_gem_scene_controls.inc']]
 files += [w/'mesa/sgx535'/name for name in ['remesa_render_target.c','remesa_render_target.h']]
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_use.h','remesa_gem_scene_use.inc']]
 files += [w/'mesa/sgx535'/name for name in ['remesa_program_registers.c','remesa_program_registers.h']]+[w/'include/remesa/scene_use_limits.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_ta.h','remesa_gem_scene_ta.inc']]+[w/'include/remesa/ta_references.h',w/'mesa/sgx535/remesa_ta_index.c',w/'mesa/sgx535/remesa_ta_index.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_states.h','remesa_gem_scene_states.inc']]+[w/'include/remesa/state_upload.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_vertices.h','remesa_gem_scene_vertices.inc']]+[w/'include/remesa/vertex_fetch.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_fragments.h','remesa_gem_scene_fragments.inc']]+[w/'include/remesa/fragment_fetch.h']
 files += [w/'kernel/remesa/gma500'/name for name in ['remesa_gem_scene_background.h','remesa_gem_scene_background.inc']]+[w/'include/remesa/background_fetch.h']
 before={str(f):sha(f) for f in files};env={k:v for k,v in os.environ.items() if k not in ('LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH')}
 def run(label,cmd,ceiling):
  def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(ceiling,ceiling))
  q=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120,preexec_fn=limit)
  (o/(label+'.log')).write_text(q.stdout+q.stderr);rows.append(dict(label=label,argv=cmd,exit=q.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
  if q.returncode or q.stderr:raise RuntimeError(label+': '+q.stderr[-2000:])
  return q.stdout
 includes=['-I'+str(o),'-I'+str(n),'-I'+str(w/'include'),'-I'+str(w/'kernel/remesa/gma500'),'-I'+str(w/'mesa/sgx535')]
 for variant,cc,flags,runner in variants:
  for name in (['test_gem_scene_actions'] if a.scene_actions else ['test_gem_dpm'] if a.dpm else ['test_gem_job_capacity','test_mmu_job_capacity']):
   exe=o/(variant+'-'+name);test=w/'kernel/remesa/gma500/tests'/(name+'.c')
   sources=[str(test)]+([str(w/'common/remesa/draw_packet.c'),str(w/'common/remesa/shader_request.c')] if name!='test_mmu_job_capacity' else [])
   if a.scene_actions:sources += [str(o/'remesa_scene_actions.c'),str(o/'remesa_program_registers.c'),str(w/'mesa/sgx535/remesa_render_target.c'),str(w/'mesa/sgx535/remesa_ta_index.c')]
   command=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers',*flags,*includes,*sources,'-o',str(exe)]
   run(variant+'-'+name+'-build',command,768*1024**2)
   out=run(variant+'-'+name+'-run',runner+[str(exe)],6*1024**3 if runner else 768*1024**2);print(out.strip(),flush=True)
 if any(sha(Path(f))!=h for f,h in before.items()):raise RuntimeError('source changed during qualification')
 (o/'RESULT.json').write_text(json.dumps(dict(status='BOUND_SCENE_ACTIONS_AND_CONTROLS_MODEL_PASS' if a.scene_actions else 'COLD_DPM_ZERO_ORIGIN_MODEL_PASS' if a.dpm else 'RETAINED_JOB_AND_MMU_CAPACITY32_MODELS_PASS',GPU_operations=0,scope='Real transformed retained core/private loan/MMU auditor with modeled kernel primitives; no native executor/GPU qualification',sources=before,generated={str(f):sha(f) for f in [o/'remesa_gem_memory.c',o/'remesa_gem_memory.h',o/'test_gem_shim.h']},commands=rows),indent=2)+'\n')
if __name__=='__main__':main()
