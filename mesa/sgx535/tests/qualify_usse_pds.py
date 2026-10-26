#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original semantic USSE/PDS and bounded constant-FS encoding; no GPU use."""
import argparse, hashlib, json, os, subprocess, shlex
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--cache',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--test',choices=['test_pds_pixel_task.c','test_usse_pds.c','test_pds_contract.c','test_usse_export_contract.c'],default='test_usse_pds.c')
a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
repo=Path(__file__).resolve().parents[3]
here=repo/'mesa/sgx535'; m=a.cache; s=m/'mesa-25.2.0'; b=m/'build-i386'
env=dict(os.environ)
env.update(json.loads((m/'build-environment.json').read_text()))
base=['-std=c11','-D_GNU_SOURCE','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DHAVE_ENDIAN_H','-DHAVE_SECURE_GETENV','-DHAVE_PTHREAD','-DHAVE_STRUCT_TIMESPEC','-UNDEBUG','-Wall','-Wextra','-Werror']
base += ['-I'+str(here)]
for inc in [s/'include',s/'src',s/'src/compiler/nir',b/'src/compiler/nir',b/'src/compiler',b/'src']:base += ['-isystem',str(inc)]
sources=[here/'remesa_usse.c',here/'remesa_pds.c',here/'remesa_shader_ir.c',here/'remesa_nir.c',here/'remesa_fragment_body.c',here/'remesa_program_registers.c',here/'tests'/a.test]
q=Path('/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386')
results=[]
for name,cc,flags,runner in [('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(m/'wrappers/cc')],['-O2','-march=i486','-static'],[str(q)])]:
 exe=a.output/name
 cmd=cc+base+flags+list(map(str,sources))+['-o',str(exe)]
 r=subprocess.run(cmd,cwd=repo,env=env,capture_output=True,text=True)
 (a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' compile failed')
 r=subprocess.run(runner+[str(exe)],env=env,capture_output=True,text=True)
 (a.output/(name+'-run.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' test failed')
 results.append({'variant':name,'command':cmd,'runner':runner+[str(exe)],'PASS':True,'exit':r.returncode,'stdout':r.stdout.strip(),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()})
dependency_inputs=set();dependency_commands=[]
for i,source in enumerate(sources):
 command=['cc']+base+['-M',str(source)]
 d=subprocess.run(command,cwd=repo,env=env,capture_output=True,text=True)
 (a.output/('dependencies-'+str(i)+'.txt')).write_text(d.stdout+d.stderr)
 if d.returncode:raise SystemExit('dependency inventory failed')
 dependency_commands.append(command)
 for name in shlex.split(d.stdout.replace('\\' + '\n',' ').split(':',1)[1]):
  path=Path(name)
  if not path.is_absolute():path=repo/path
  dependency_inputs.add(str(path.resolve()))
(a.output/'compiler-inputs.json').write_text(json.dumps({'scope':'Compiler-reported input paths, not syscall audit','commands':dependency_commands,'files':sorted(dependency_inputs)},indent=2)+'\n')
paths=sources+[here/'remesa_usse.h',here/'remesa_pds.h',here/'remesa_shader_ir.h',here/'remesa_nir.h',Path(__file__).resolve(),m/'build-environment.json']
(a.output/'result.json').write_text(json.dumps({'status':'PASS','classification':'OFFLINE QUALIFIED','scope':'Original HOST semantic schema, lowering, transport and PDS relocation metadata. Bounded original magenta/green FS body encoding supported; unsupported USSE/PDS profiles still return ENOTSUP. Zero SGX instructions executed. Round trips do not prove hardware correctness.','processes':len(results),'tests':results,'sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths}},indent=2)+'\n')
print(json.dumps(results,indent=2))
