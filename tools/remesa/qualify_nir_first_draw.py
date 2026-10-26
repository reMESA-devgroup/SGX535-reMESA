#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the real NIR/Gallium caller; test simulated Linux transport under QEMU.
No device discovery, GPU submission or physical completion evidence in this suite.
"""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--reference-root',type=Path,help='Explicit repository containing the preserved pixel-output reference and execution card; never a shader payload')
 a=p.parse_args()
 a.output.mkdir(parents=True,exist_ok=False)
 repo=Path(__file__).resolve().parents[2];m=a.cache;b=m/'build-i386';env=dict(os.environ);env.update(json.loads((m/'build-environment.json').read_text()))
 record=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/remesa_nir.c'))
 args=record.get('arguments') or shlex.split(record['command']);base=[];skip=False
 for arg in args:
  if skip:skip=False;continue
  if arg in ('-o','-MF','-MQ'):skip=True;continue
  if arg in ('-c','-MD') or arg==record['file']:continue
  base.append(arg)
 sources=[repo/'mesa/sgx535/tests/gallium_nir_first_draw.c']+[repo/'mesa/sgx535'/x for x in ['remesa_fixed_state.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_screen.c','remesa_draw.c','remesa_retained_transport.c','remesa_linux.c','remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c']]+[repo/'common/remesa/draw_packet.c',repo/'common/remesa/retained_evidence.c',repo/'common/remesa/shader_request.c']
 sources += [repo/'mesa/sgx535'/x for x in ['remesa_persistent.c','remesa_linux_memory.c']]+[repo/'kernel/remesa'/x for x in ['registry.c','resource.c','provider.c']]
 common=base+list(map(str,sources))+['-I'+str(repo/'include'),'-I'+str(repo/'mesa/sgx535'),'-Wl,--start-group']+[str(b/x) for x in ['src/gallium/auxiliary/libgallium.a','src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a']]+['-Wl,--end-group',str(m/'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),'-lm','-ldl','-latomic','-pthread']
 for name,extra in [('gallium-nir-first-draw-i386',[]),('simulated-client-i386',[str(repo/'mesa/sgx535/tests/nir_first_draw_transport_fixture.c'),'-Wl,--wrap=open64','-Wl,--wrap=close','-Wl,--wrap=ioctl'])]:
  cmd=common+extra+['-o',str(a.output/name)];r=subprocess.run(cmd,cwd=b,env=env,text=True,capture_output=True);(a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr);(a.output/(name+'-command.json')).write_text(json.dumps(cmd,indent=2)+'\n')
  if r.returncode:raise RuntimeError('compile failed: '+name)
 reference_root=a.reference_root.resolve() if a.reference_root else repo
 expected=json.loads((reference_root/'docs/architecture/retained-gallium-triangle-execution-card-20261008.json').read_text())['expected_framebuffer'];ref=reference_root/expected['path'];raw=ref.read_bytes()
 if len(raw)!=4096 or hashlib.sha256(raw).hexdigest()!=expected['sha256']:raise RuntimeError('sealed reference mismatch')
 env['REMESA_SIMULATED_REFERENCE']=str(ref)
 q=Path('/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386');results=[]
 for mode in ['success','pixel-mismatch','missing-completion','wrong-operation','interrupted','positive-ioctl']:
  env['REMESA_SIMULATED_OUTCOME']=mode;out=a.output/(mode+'.bin')
  r=subprocess.run([str(q),'-L',str(m/'runtime'),str(a.output/'simulated-client-i386'),'--execute-once','SIMULATED-NO-DEVICE',str(out),str(ref)],env=env,text=True,capture_output=True,timeout=15)
  (a.output/(mode+'.log')).write_text(r.stdout+r.stderr)
  ok=(r.returncode==0)==(mode=='success') and r.stderr.count('submissions=1')==1
  if mode=='success':ok=ok and out.read_bytes()==raw
  if mode=='pixel-mismatch':ok=ok and len(out.read_bytes())==4096 and out.read_bytes()!=raw
  results.append(dict(case=mode,returncode=r.returncode,PASS=ok,scope='SIMULATED ioctl fixture; no hardware'))
  if not ok:raise RuntimeError('caller regression: '+mode)
 (a.output/'result.json').write_text(json.dumps(dict(PASS=True,cases=results,reference_root=str(reference_root),reference_path=str(ref),reference_sha256=expected['sha256'],scope='Actual Mesa NIR builder/callbacks and transport; simulated responses only'),indent=2)+'\n')
 print('PASS: 6 i386 actual-NIR caller/transport/readback outcomes; simulated only')
if __name__=='__main__':main()
