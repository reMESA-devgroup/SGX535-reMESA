from pathlib import Path
import subprocess,json,hashlib
N=Path(__file__).resolve().parent;rows=[]
s=(N/'module/remesa_stream_trusted.c').read_text();a=s.index('static int check_images(');b=s.index('static int verify_borrower(',a);(N/'tests/check_images_actual.inc').write_text(s[a:b]);M=N.parent/'phase9-real-mesa-20261008';Q=N.parent/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
def run(name,cmd):
 p=subprocess.run([str(x)for x in cmd],cwd=N,capture_output=True,text=True);(N/(name+'.log')).write_text(p.stdout+p.stderr);assert not p.returncode,(name,p.stderr[-3000:]);rows.append({'name':name,'exit':0,'command':[str(x)for x in cmd]});print(name,'PASS',flush=True)
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',M/'wrappers/cc',['-O2','-static','-march=i486'],[Q])]:
 common=[cc,'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I'+str(N/'module'),'-I/home/gama/sgx535-gfx/include',*flags]
 exe=N/('test-nohash-inputs-'+mode);run(mode+'-nohash-inputs-build',common+[N/'tests/test_nohash_inputs.c',N/'module/remesa_stream_grammar.c','-o',exe]);run(mode+'-nohash-inputs',runner+[exe,N/'stream-profile'])
 exe=N/('test-nohash-source-'+mode);run(mode+'-nohash-source-build',common+['-Wno-unused-parameter',N/'tests/test_source.c',N/'module/frozen_capsule.c',N/'module/frozen_source_guard.c','-o',exe])
 for case in ['twoframes','pending','producer','wrongtask','latepending','overflow','wrongseq','terminalpending','nohash','nohash_wrongledger','nohash_wrongphase','nohash_pending']:run(mode+'-source-'+case,runner+[exe,case])
(N/'NOHASH_TRUSTED_TEST_RESULTS.json').write_text(json.dumps({'GPU_operations':0,'remote_calls':0,'scope':'Actual input authentication and source lifetime code with fake provider/MMIO/task/lock callbacks; not GPU proof','tests':rows},indent=2)+'\n')
