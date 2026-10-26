from pathlib import Path
import subprocess,json
N=Path(__file__).resolve().parent;s=(N/'module/remesa_stream_trusted.c').read_text();parts=[]
for start,end in [('static int execute(','static int settle('),('static int seal(','static void quarantine(')]:parts.append(s[s.index(start):s.index(end)])
(N/'tests/nohash_output_actual.inc').write_text('\n'.join(parts));rows=[];M=N.parent/'phase9-real-mesa-20261008';Q=N.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 exe=N/('test-nohash-output-'+mode);cmd=[str(cc),'-std=gnu99','-Wall','-Wextra','-Werror',*flags,str(N/'tests/test_nohash_output_paths.c'),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);assert not p.returncode,p.stderr
 p=subprocess.run([*map(str,runner),str(exe)],capture_output=True,text=True);assert not p.returncode,p.stderr;(N/(mode+'-nohash-output.log')).write_text(p.stdout+p.stderr);rows.append({'mode':mode,'exit':0,'command':cmd,'result':p.stdout.strip()});print(mode,'PASS',flush=True)
(N/'NOHASH_OUTPUT_TEST_RESULTS.json').write_text(json.dumps({'GPU_operations':0,'remote_calls':0,'scope':'Actual execute/seal functions, stub provider/visible/source calls; validates hash branch selection, not GPU execution or real visibility','tests':rows},indent=2)+'\n')
