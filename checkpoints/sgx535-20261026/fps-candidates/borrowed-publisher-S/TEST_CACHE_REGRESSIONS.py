from pathlib import Path
import subprocess,json
C=Path(__file__).resolve().parent;M=C.parent/'phase9-real-mesa-20261008';Q=C.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 for name in ['test_ta_seal','test_cached_views','test_stream_memory','test_gem_regression','test_production_input_seal']:
  exe=C/(name+'-'+mode);cmd=[str(cc),'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I'+str(C/'tests'),'-I/home/gama/sgx535-gfx/include',*flags,str(C/'tests'/f'{name}.c'),*([str(C/'module/remesa_stream_grammar.c')]if name=='test_production_input_seal'else[]),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);assert not p.returncode,p.stderr[-3000:]
  p=subprocess.run([*map(str,runner),str(exe)],capture_output=True,text=True);(C/(name+'-'+mode+'.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr;rows.append({'mode':mode,'name':name,'exit':0,'command':cmd,'stdout':p.stdout});print(mode,name,'PASS',flush=True)
(C/'CACHE_REGRESSION_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'scope':'Actual provider and existing safety regressions with fake kernel/PFN APIs; CPU only','tests':rows},indent=2)+'\n')
