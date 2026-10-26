#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only qualification of the GEM held-job readback integration fragment."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--endpoint',action='store_true',help='Qualify the default-off read-only endpoint envelope');p.add_argument('--native-source',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
w=Path(__file__).resolve().parents[2];a.output.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.update(json.loads((a.cache/'build-environment.json').read_text()))
q='/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
inputs=[a.native_source/n for n in ('remesa_gem_memory.c','remesa_gem_memory.h','test_gem_shim.h')]+[w/n for n in ('kernel/remesa/gma500/remesa_gem_visible_readback.inc','kernel/remesa/gma500/remesa_gem_visible_readback.h','kernel/remesa/gma500/tests/test_visible_readback.c')]
test=inputs[-1]
if a.endpoint:
 inputs += [w/n for n in ('include/remesa/visible_readback_uapi.h','kernel/remesa/gma500/remesa_gem_visible_ioctl.inc','kernel/remesa/gma500/remesa_gem_visible_ioctl.h','kernel/remesa/gma500/tests/test_visible_ioctl.c')]
 test=inputs[-1]
identity={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}
results={}
for name,cc,flags,runner in [('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(a.cache/'wrappers/cc')],['-O2','-static','-march=i486'],[q])]:
 exe=a.output/name;cmd=cc+['-std=c11','-Wall','-Wextra','-Werror','-I'+str(w/'include'),'-I'+str(a.native_source)]+flags+[str(test),'-o',str(exe)]
 r=subprocess.run(cmd,env=env,text=True,capture_output=True);(a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
 runs=[]
 for mode in range(4):
  command=runner+[str(exe),str(mode)];r=subprocess.run(command,env=env,capture_output=True,text=True,timeout=60);(a.output/(name+'-'+str(mode)+'.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
  runs.append({'mode':mode,'command':command,'exit':r.returncode,'output':r.stdout.strip()})
 results[name]={'pass':True,'command':cmd,'runs':runs,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};(a.output/'RESULTS.partial.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(hashlib.sha256(Path(f).read_bytes()).hexdigest()==h for f,h in identity.items())
(a.output/'TEST_RESULTS.json').write_text(json.dumps({'CPU_ONLY':True,'endpoint_envelope':a.endpoint,'GPU_submissions':0,'source_sha256':identity,'variants':results},indent=2)+'\n');print('PASS CPU_ONLY: four held-job readback controls in native/UBSan/i386; zero GPU')
