#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Actual Mesa constructors -> immutable images -> actual kernel scene checks.
All device/kernel primitives and MMIO callbacks remain CPU models, GPU0.
"""
import argparse,hashlib,json,os,resource,shutil,struct,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--producer',type=Path,required=True);p.add_argument('--kernel-tests',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 w=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ);env.pop('REMESA_CAPTURE_EXPORT_DIR',None);env.pop('REMESA_CAPTURE_FINITE_VERTICES',None)
 qemu=Path('/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386');rows=[]
 def run(label,argv,extra=None,refusal=False):
  e=dict(env);e.update(extra or {})
  def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(6*1024**3,)*2)
  q=subprocess.run(argv,env=e,capture_output=True,text=True,timeout=120,preexec_fn=limit)
  (o/(label+'.log')).write_text(q.stdout+q.stderr);rows.append({'label':label,'argv':argv,'exit':q.returncode,'expected_refusal':refusal})
  (o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
  if refusal:assert q.returncode and 'execute=' in q.stdout and 'prepare=0' in q.stdout,(label,q.stdout,q.stderr)
  else:assert q.returncode==0 and not q.stderr,(label,q.stdout,q.stderr)
 variants=[('native',[]),('ubsan',[]),('i486',[str(qemu)])]
 for profile in ['original-inputs','finite-inputs']:
  root=o/profile;root.mkdir();extra={'REMESA_CAPTURE_EXPORT_DIR':str(root)}
  if profile=='finite-inputs':extra['REMESA_CAPTURE_FINITE_VERTICES']='1'
  run(profile+'-export',[str(a.producer.resolve())],extra)
  for size in ['1280x800','640x640','641x479']:
   for variant,runner in variants:
    run(profile+'-'+size+'-'+variant,runner+[str(a.kernel_tests.resolve()/(variant+'-test_gem_scene_actions')),'--replay',str(root/size)])
 # Faults alter only independent copies of the finite fixture. Never touch the
 # retained producer files. Preparation still succeeds; actual deep image or
 # index bounds gates must refuse before any backend stage executes.
 base=o/'finite-inputs/1280x800';packet=(base/'packet.bin').read_bytes();placement=(base/'placement.bin').read_bytes()
 use=[struct.unpack_from('<6I',packet,400+24*i) for i in range(struct.unpack_from('<I',packet,24)[0])]
 shader=next(e for e in use if e[2]==64 and e[4]==0)
 ids=[struct.unpack_from('<I',placement,8+24*i+16)[0] for i in range(17)]
 index=struct.unpack_from('<8I',packet,1720+32)
 for fault in ['shader-opcode','index-out-of-range']:
  dst=o/fault;shutil.copytree(base,dst)
  if fault=='shader-opcode':
   path=dst/('bo-'+str(ids.index(shader[0]))+'.bin');b=bytearray(path.read_bytes());b[shader[1]+7]^=0x80;path.write_bytes(b)
  else:
   path=dst/('bo-'+str(ids.index(index[0]))+'.bin');b=bytearray(path.read_bytes());struct.pack_into('<H',b,index[1],index[6]);path.write_bytes(b)
  for variant,runner in variants:run(fault+'-'+variant,runner+[str(a.kernel_tests.resolve()/(variant+'-test_gem_scene_actions')),'--replay',str(dst)],refusal=True)
 paths=[w/'tools/remesa/test_scene_replay.py',w/'tools/remesa/scene_fixture.h',w/'mesa/sgx535/tests/scene_capture_export.inc',w/'mesa/sgx535/tests/test_scene_capture.c',w/'mesa/sgx535/tests/test_linked_background.c',w/'kernel/remesa/gma500/tests/scene_replay_model.inc',w/'kernel/remesa/gma500/tests/test_gem_scene_actions.c',w/'kernel/remesa/gma500/tests/test_gem_private.c']
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 report={'status':'ACTUAL_MESA_IMAGES_THROUGH_GENERAL_KERNEL_SCENE_MODEL_PASS','GPU_operations':0,'successful_replays':18,'deep_refusals':6,'exports':2,'scope':'Actual original encoders/image bytes, real memory lease/write/job/closure/executor/retirement; MMU/source/backend hardware callbacks modeled, no physical GPU qualification','sources':{str(p):sha(p) for p in paths},'binaries':{str(p):sha(p) for p in [a.producer.resolve()]+[a.kernel_tests.resolve()/(v+'-test_gem_scene_actions') for v,_ in variants]},'artifacts':{str(p):sha(p) for p in sorted(o.rglob('*')) if p.is_file() and p.suffix=='.bin'},'commands':rows}
 (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
