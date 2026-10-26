from pathlib import Path
import subprocess,json
C=Path(__file__).resolve().parent;M=C.parent/'phase9-real-mesa-20261008';Q=C.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';T=C/'tests'
s=(C/'module/remesa_stream_trusted.c').read_text();(T/'trusted_prefire_actual.inc').write_text(s[s.index('int remesa_stream_trusted_prefire_cache('):s.index('static int check_images(')])
a=s.index('static int bind_publisher_images(');b=s.index('/* End publisher image lifetime helpers. */',a);(T/'publisher_views_actual.inc').write_text(s[a:b])
s=(C/'module/remesa_stream_owner_flush.c').read_text();(T/'owner_flush_actual.inc').write_text(s[s.index('int remesa_stream_owner_flush('):]);rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 exe=C/('test-ta-cache-domains-'+mode);cmd=[str(cc),'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I'+str(T),'-I/home/gama/sgx535-gfx/include',*flags,str(T/'test_ta_cache_domains.c'),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);(C/(mode+'-cache-domains-build.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr[-3500:]
 p=subprocess.run([*map(str,runner),str(exe)],capture_output=True,text=True);(C/(mode+'-cache-domains-run.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr;rows.append({'mode':mode,'exit':0,'command':cmd,'stdout':p.stdout,'GPU_operations':0});print(mode,'PASS',flush=True)
(C/'CACHE_DOMAINS_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'scope':'Actual provider, owner_flush and trusted bridge with synthetic full PFN metadata and one-page CPU/GPU cache model per BO; no hardware coherence claim','tests':rows},indent=2)+'\n')
