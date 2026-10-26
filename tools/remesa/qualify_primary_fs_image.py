#!/usr/bin/env python3
"""CPU-only primary FS image regression against an authenticated lab capture.
Does not authorize a workload, connect to the Mini12, open DRM, or submit work.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
W=Path(__file__).resolve().parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();receipt=json.loads((a.capture/'ACTUAL-VERIFICATION.json').read_text());e=a.capture/'actual-mini12/evidence';color=(e/'gallium-readback.bin').read_bytes();assert sha(color)==receipt['gallium_readback_sha256'];assert receipt['same_job']['SGX_phase']==9 and receipt['same_job']['GEM_state']==6
j=json.loads((e/'dynamic-memory.json').read_text());capture=json.loads((e/'capture.json').read_text());pds_id=str(capture['roles'][0]['id']);observed=bytes.fromhex(j['memory']['before'][pds_id]['data'])[0x160:0x198]
assert len(observed)==56
Q=Path('/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386');env=dict(os.environ);env.update(json.loads((a.cache/'build-environment.json').read_text()));rows=[]
variants=[('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(a.cache/'wrappers/cc')],['-O2','-static','-march=i486'],[str(Q)])]
for name,cc,flags,run in variants:
 exe=a.output/name;cmd=cc+['-std=c11','-Wall','-Wextra','-Werror','-I'+str(W/'mesa/sgx535')]+flags+[str(W/'tools/remesa/pds_primary_image.c'),str(W/'mesa/sgx535/remesa_pds_structure.c'),str(W/'mesa/sgx535/remesa_program_registers.c'),'-o',str(exe)];r=subprocess.run(cmd,env=env,capture_output=True);(a.output/(name+'-build.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 cases=[]
 for address,bank,ok in [('0x20080000','3',True),('0x20080000','4',True),('0x20080001','3',False),('0x20080000','16',False),('bad','3',False),('0x100000000','3',False)]:
  command=run+[str(exe),address,bank];r=subprocess.run(command,env=env,capture_output=True);assert (r.returncode==0)==ok
  if ok:
   assert len(r.stdout)==56
   if bank=='3':assert r.stdout==observed
   else:assert r.stdout[4:]==observed[4:] and int.from_bytes(r.stdout[:4],'little')==4
  else:assert not r.stdout
  cases.append({'command':command,'exit':r.returncode,'bytes':len(r.stdout),'sha256':sha(r.stdout)})
 rows.append({'variant':name,'compile':cmd,'cases':cases,'binary_sha256':sha(exe.read_bytes())})
sources=[W/'tools/remesa/pds_primary_image.c',W/'mesa/sgx535/remesa_pds_structure.c',W/'mesa/sgx535/remesa_pds_structure.h',W/'mesa/sgx535/remesa_program_registers.c',W/'mesa/sgx535/remesa_program_registers.h']
result={'status':'PASS_CPU_REGRESSION_AGAINST_PHYSICAL_CAPTURE','hardware_executed_by_this_runner':False,'capture':str(a.capture),'capture_receipt_sha256':sha((a.capture/'ACTUAL-VERIFICATION.json').read_bytes()),'physical_primary_image_sha256':sha(observed),'scope':'Exact primary FS CPU image only; no generic ISA, VS or launch admission','runs':rows,'sources':{str(s):sha(s.read_bytes())for s in sources}}
(a.output/'TEST_RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS 6 cases per native/UBSan/i386; independent56byte image equals authenticated physical memory')
