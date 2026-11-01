#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Portable userspace provider capacity/lifetime tests; no kernel/GPU access."""
import argparse,hashlib,json,os,resource,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--i486-cc',type=Path);p.add_argument('--qemu-i386',type=Path);p.add_argument('--environment-file',type=Path);a=p.parse_args()
 if bool(a.i486_cc)!=bool(a.qemu_i386):p.error('supply both i486 compiler and qemu')
 w=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ)
 if a.environment_file:env.update(json.loads(a.environment_file.read_text()))
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if a.i486_cc:variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
 rows=[];sources=[w/'kernel/remesa'/n for n in ['resource.c','registry.c','provider.c','tests/test_provider_capacity.c']]
 for name,cc,flags,runner in variants:
  exe=o/name;cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(w/'include'),*[str(p) for p in sources],'-o',str(exe)]
  for label,argv in [(name+'-build',cmd),(name+'-run',runner+[str(exe)])]:
   def limits():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,((6*1024**3 if runner else 768*1024**2),)*2)
   q=subprocess.run(argv,env=env,capture_output=True,timeout=90,preexec_fn=limits);(o/(label+'.log')).write_bytes(q.stdout+q.stderr);rows.append({'label':label,'argv':argv,'exit':q.returncode});(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
   if q.returncode or q.stderr:raise RuntimeError(label+' failed; see preserved logs')
 paths=sources+[Path(__file__).resolve()]+[w/'include/remesa'/n for n in ['provider.h','registry.h','resource.h']]
 report={'status':'PORTABLE_PROVIDER_SCENE_CAPACITY_LIFETIME_PASS','GPU_operations':0,'variants':len(variants),'counts':[1,16,17,32],'invalid_count':33,'scope':'Real portable ownership core, modeled device callbacks; no native admission, MMIO, kernel stack or physical completion claim','sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'commands':rows};(o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
