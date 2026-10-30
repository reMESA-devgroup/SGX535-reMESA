#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual backing-session helper, close, loss and admission; CPU metadata model."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from test_private_source_mmio import extract

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--module',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path)
 a=p.parse_args();w=Path(__file__).resolve().parents[2];m=a.module.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
 source=(m/'remesa_gem_memory.c').read_text()
 (o/'memory_close_actual.inc').write_text(extract(source,'remesa_gem_memory_close'))
 act=extract(source,'act');start=act.index(' if(s->stream_bound || s->private_session) {');brace=act.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(act[end]=='{')-(act[end]=='}');end+=1
 block=act[start:end]
 (o/'session_gate_actual.inc').write_text('static int session_gate(struct remesa_gem_memory *s,struct remesa_memory_request *r)\n{unsigned slot;\n'+block+'\nreturn 0;\n}\n')
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc or a.qemu_i386:
  if not (a.i486_cc and a.qemu_i386):p.error('both i486 options required')
  variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 rows=[]
 def limit(n):
  resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(n,n))
 test=w/'kernel/remesa/gma500/tests/test_private_session.c'
 for name,cc,flags,runner in variants:
  exe=o/name
  command=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(o),'-I'+str(m),str(test),'-o',str(exe)]
  for label,argv,ceiling in [(name+'-build',command,768*1024**2),(name+'-run',runner+[str(exe)],6*1024**3 if runner else 768*1024**2)]:
   r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limit(ceiling));(o/(label+'.log')).write_text(r.stdout+r.stderr)
   rows.append(dict(label=label,argv=argv,exit=r.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
   if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
 files=[Path(__file__).resolve(),test,m/'remesa_gem_memory.c',m/'remesa_private_session.inc',m/'remesa_private_session.h',m/'remesa_private_memory_file_lost.inc',o/'session_gate_actual.inc',o/'memory_close_actual.inc']
 result=dict(status='PRIVATE_BACKING_SESSION_METADATA_PASS',GPU_operations=0,scope='actual metadata/loss/close/admission code; jobs, kernel primitives and CPU leases modeled; no execution/source lease or retirement qualification',sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
 (o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':result['status'],'variants':len(variants)}))
if __name__=='__main__':main()
