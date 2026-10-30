#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual memory service/private seals and original PDS guard; no GPU."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
import stage_twod_reservation as stage

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--native',type=Path,required=True);p.add_argument('--shim',type=Path,required=True)
 p.add_argument('--output',type=Path,required=True);p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path)
 a=p.parse_args();w=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
 native=a.native.resolve();raw=(native/'remesa_gem_memory.c').read_bytes()
 if hashlib.sha256(raw).hexdigest()!=stage.PINNED['remesa_gem_memory.c']:raise ValueError('unreviewed native memory source')
 (o/'private_memory_actual.c').write_text(stage.private_file_lost_sources(raw.decode()))
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc or a.qemu_i386:
  if not(a.i486_cc and a.qemu_i386):p.error('both i486 options required')
  variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 rows=[];test=w/'kernel/remesa/gma500/tests/test_private_inputs.c'
 for name,cc,flags,runner in variants:
  exe=o/name;cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers',*flags,
    '-I'+str(o),'-I'+str(a.shim.resolve().parent),'-I'+str(native),'-I'+str(w/'include'),'-I'+str(w/'kernel/remesa/gma500'),str(test),'-o',str(exe)]
  for label,argv in [(name+'-build',cmd),(name+'-run',runner+[str(exe)])]:
   def limit():
    n=6*1024**3 if runner and label.endswith('-run') else 768*1024**2
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(n,n))
   r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limit);(o/(label+'.log')).write_text(r.stdout+r.stderr)
   rows.append(dict(label=label,argv=argv,exit=r.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
   if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
 files=[Path(__file__).resolve(),test,a.shim,native/'remesa_gem_memory.c',native/'remesa_pds_write_guard.h',o/'private_memory_actual.c']
 files += [w/'kernel/remesa/gma500'/n for n in ['remesa_private_inputs.inc','remesa_private_session.inc','remesa_private_session_source.inc']]
 result=dict(status='ACTUAL_PRIVATE_INPUT_SEALS_AND_MEMORY_TRANSFERS_PASS',GPU_operations=0,scope='actual memory request/job/lease/seal/transfer/release and original PDS guard; GEM/MMU/PM/source/initial authentication modeled; not hardware or shader qualification',sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
 (o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':result['status'],'variants':len(variants)}))
if __name__=='__main__':main()
