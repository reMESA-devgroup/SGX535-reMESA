from pathlib import Path
import subprocess,json
D=Path(__file__).resolve().parent;M=D.parent/'phase9-real-mesa-20261008';Q=D.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 exe=D/('test-ta-borrowed-lease-'+mode);cmd=[str(cc),'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I'+str(D/'tests'),'-I/home/gama/sgx535-gfx/include',*flags,str(D/'tests/test_ta_borrowed_lease.c'),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);(D/(mode+'-borrowed-lease-build.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr[-3000:]
 p=subprocess.run([*map(str,runner),str(exe)],capture_output=True,text=True);(D/(mode+'-borrowed-lease-run.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr;rows.append({'mode':mode,'exit':0,'command':cmd,'stdout':p.stdout});print(mode,'PASS',flush=True)
(D/'BORROWED_TA_LEASE_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'scope':'Actual provider and real heap-backed fake GEM pages; synthetic mappings/vmap-budget failure, no hardware coherence proof','tests':rows},indent=2)+'\n')
