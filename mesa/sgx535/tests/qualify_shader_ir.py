#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original semantic IR validation and real Mesa NIR lowering; no SGX encoding."""
import argparse,json,os,subprocess,shlex,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[3];m=a.cache;s=m/'mesa-25.2.0';b=m/'build-i386';env=dict(os.environ);env.update(json.loads((m/'build-environment.json').read_text()))
base=['-std=c11','-D_GNU_SOURCE','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DHAVE_ENDIAN_H','-DHAVE_SECURE_GETENV','-DHAVE_PTHREAD','-DHAVE_STRUCT_TIMESPEC','-UNDEBUG']
base+=['-I'+str(x) for x in [repo/'include',repo/'mesa/sgx535',s/'include',s/'src',s/'src/compiler/nir',b/'src/compiler/nir',b/'src/compiler',b/'src']]
sources=[repo/'common/remesa/shader_request.c',repo/'mesa/sgx535/remesa_shader_ir.c',repo/'mesa/sgx535/remesa_usse.c',repo/'mesa/sgx535/remesa_pds.c',repo/'mesa/sgx535/remesa_pds_structure.c',repo/'mesa/sgx535/remesa_shader_program.c',repo/'mesa/sgx535/remesa_usse_fragment_color.c',repo/'mesa/sgx535/remesa_usse_arithmetic.c',repo/'mesa/sgx535/remesa_fragment_body.c',repo/'mesa/sgx535/remesa_shader_storage.c',repo/'mesa/sgx535/remesa_program_registers.c',repo/'mesa/sgx535/remesa_nir.c',repo/'mesa/sgx535/remesa_nir_uniform.c',b/'src/compiler/nir/nir_intrinsics.c',s/'src/util/ralloc.c',s/'src/util/u_printf.c']
q='/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';results={}
def run(name,cmd,cwd,runner):
 r=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True);(a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' compile failed')
 r=subprocess.run(runner,env=env,capture_output=True,text=True);(a.output/(name+'-run.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' test failed')
 results[name]={'PASS':True,'command':cmd,'runner':runner,'output':r.stdout.strip()}
for name,cc,flags,runner in [('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(m/'wrappers/cc')],['-O2','-march=i486','-static'],[q])]:
 exe=a.output/name;run(name,cc+base+flags+list(map(str,sources))+[str(repo/'mesa/sgx535/tests/test_shader_ir.c'),'-o',str(exe)],repo,runner+[str(exe)])
record=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/remesa_nir.c'));args=record.get('arguments') or shlex.split(record['command']);filtered=[];skip=False
for arg in args:
 if skip:skip=False;continue
 if arg in ('-o','-MF','-MQ'):skip=True;continue
 if arg in ('-c','-MD') or arg==record['file']:continue
 filtered.append(arg)
filtered.insert(1,'-I'+str(repo/'mesa/sgx535'));filtered.append('-UNDEBUG');exe=a.output/'actual-builder-i386'
cmd=filtered+[str(repo/'common/remesa/shader_request.c'),str(repo/'mesa/sgx535/tests/test_shader_ir_builder.c'),str(repo/'mesa/sgx535/remesa_shader_ir.c'),str(repo/'mesa/sgx535/remesa_usse.c'),str(repo/'mesa/sgx535/remesa_pds.c'),str(repo/'mesa/sgx535/remesa_pds_structure.c'),str(repo/'mesa/sgx535/remesa_shader_program.c'),str(repo/'mesa/sgx535/remesa_usse_fragment_color.c'),str(repo/'mesa/sgx535/remesa_usse_arithmetic.c'),str(repo/'mesa/sgx535/remesa_fragment_body.c'),str(repo/'mesa/sgx535/remesa_shader_storage.c'),str(repo/'mesa/sgx535/remesa_program_registers.c'),str(repo/'mesa/sgx535/remesa_nir.c'),str(repo/'mesa/sgx535/remesa_nir_uniform.c'),'-Wl,--start-group']+[str(b/x) for x in ['src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a']]+['-Wl,--end-group',str(m/'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),'-lm','-ldl','-latomic','-pthread','-o',str(exe)]
run('actual-builder-i386',cmd,b,[q,'-L',str(m/'runtime'),str(exe)])
identities={str(path.relative_to(repo)):hashlib.sha256(path.read_bytes()).hexdigest() for path in [repo/'mesa/sgx535/remesa_shader_ir.c',repo/'mesa/sgx535/remesa_usse.c',repo/'mesa/sgx535/remesa_pds.c',repo/'mesa/sgx535/remesa_pds_structure.c',repo/'mesa/sgx535/remesa_shader_program.c',repo/'mesa/sgx535/remesa_usse_fragment_color.c',repo/'mesa/sgx535/remesa_usse_arithmetic.c',repo/'mesa/sgx535/remesa_fragment_body.c',repo/'mesa/sgx535/remesa_shader_storage.c',repo/'mesa/sgx535/remesa_program_registers.c',repo/'mesa/sgx535/remesa_shader_ir.h',repo/'mesa/sgx535/remesa_nir_uniform.c',repo/'mesa/sgx535/remesa_nir_uniform.h',repo/'mesa/sgx535/remesa_nir.c',repo/'mesa/sgx535/tests/test_shader_ir.c',repo/'mesa/sgx535/tests/test_shader_ir_builder.c',repo/'mesa/sgx535/tests/vertex_nir_fixture.h',Path(__file__).resolve()]}
(a.output/'result.json').write_text(json.dumps({'scope':'original host semantic IR; no SGX encoding or execution; encoder fails ENOTSUP','sources':identities,'tests':results},indent=2)+'\n');print(json.dumps(results,indent=2))
