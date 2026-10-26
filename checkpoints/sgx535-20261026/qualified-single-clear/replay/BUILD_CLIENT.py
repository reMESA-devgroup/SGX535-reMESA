from pathlib import Path
import subprocess,json,hashlib
R=Path(__file__).resolve().parent;N=R.parent;M=N.parent/'phase9-real-mesa-20261008';Q=N.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 exe=R/('stream-probe-'+mode);cmd=[str(cc),'-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(R),'-I'+str(N/'module'),'-I/home/gama/sgx535-gfx/include',*flags,str(R/'stream_probe.c'),str(R/'frame_update.c'),str(N/'module/remesa_stream_grammar.c'),'-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);(R/(mode+'-build.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr[-3000:]
 for args in [['--self-test'],['--describe'],['--check-updates',str(N/'stream-profile'),str(R/'updates'),'6']]:
  p=subprocess.run([*map(str,runner),str(exe),*args],capture_output=True,text=True);assert not p.returncode,p.stderr;rows.append({'mode':mode,'args':args,'exit':0,'stdout':p.stdout,'GPU_operations':0})
 print(mode,'PASS',flush=True)
(R/'CLIENT_BUILD_RESULTS.json').write_text(json.dumps({'GPU_operations':0,'client_SHA256':hashlib.sha256((R/'stream-probe-i386').read_bytes()).hexdigest(),'scope':'Separate OPEN8 ABI5 no-crypto raw qualification client, not production FAST or FPS benchmark','tests':rows},indent=2)+'\n')
