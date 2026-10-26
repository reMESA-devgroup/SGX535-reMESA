from pathlib import Path
import subprocess,json,hashlib
D=Path(__file__).resolve().parent;N=D.parent/'gears-fast-nohash-20261025T223500Z';M=D.parent/'phase9-real-mesa-20261008';Q=D.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
s=(N/'module/mmu.c').read_text()
def function(signature):
 a=s.index(signature);b=s.index('{',a);depth=0
 for i in range(b,len(s)):
  if s[i]=='{':depth+=1
  elif s[i]=='}':
   depth-=1
   if not depth:return s[a:i+1]+'\n'
 raise AssertionError(signature)
chunks=[function(x)for x in ['static inline uint32_t psb_mmu_pt_index','static inline uint32_t psb_mmu_pd_index','static struct psb_mmu_pt *psb_mmu_pt_map_lock','static void psb_mmu_pt_unmap_unlock','int psb_mmu_virtual_to_pfn']]
(D/'tests/actual_mmu_reference.inc').write_text('\n'.join(chunks));rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',str(M/'wrappers/cc'),['-O2','-static','-march=i486'],[str(Q)])]:
 cmd=[cc,'-std=c99','-Wall','-Wextra','-Werror',*flags,str(D/'tests/test_mmu_verify_pages.c'),'-o',str(D/('test-pte-'+mode))];p=subprocess.run(cmd,capture_output=True);(D/(mode+'-pte-build.log')).write_bytes(p.stdout+p.stderr);assert not p.returncode,p.stderr.decode()
 p=subprocess.run([*runner,str(D/('test-pte-'+mode))],capture_output=True);(D/(mode+'-pte-run.log')).write_bytes(p.stdout+p.stderr);assert not p.returncode,p.stderr.decode();print(mode,p.stdout.decode().strip(),flush=True);rows.append({'mode':mode,'exit':p.returncode,'command':cmd,'stdout':p.stdout.decode()})
(D/'PTE_BATCH_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_submissions':0,'remote_calls':0,'reference_source':str(N/'module/mmu.c'),'reference_sha256':hashlib.sha256(s.encode()).hexdigest(),'actual_batch_sha256':hashlib.sha256((D/'module/remesa_mmu_verify_pages.inc').read_bytes()).hexdigest(),'scope':'Actual batch implementation and extracted actual old MMU functions against synthetic PT/lock/map shim; not physical MMU proof','tests':rows},indent=2)+'\n')
