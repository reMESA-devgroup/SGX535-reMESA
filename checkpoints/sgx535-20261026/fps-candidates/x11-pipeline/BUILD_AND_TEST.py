from pathlib import Path
import json,subprocess,os,fcntl,hashlib
D=Path(__file__).resolve().parent;O=D.parent;N=O/'gears-fast-nohash-20261025T223500Z';M=O/'phase9-real-mesa-20261008';W=Path('/home/gama/sgx535-worktrees/driver-resume-20261018');env=dict(os.environ);env.update(json.loads((M/'build-environment.json').read_text()));old=json.loads((N/'FRONTEND_BUILD_RESULTS.json').read_text())['commands'];rows=[]
def run(label,cmd,cwd=M/'build-i386',environment=env):
 p=subprocess.run(cmd,cwd=cwd,env=environment,capture_output=True);(D/(label+'.log')).write_bytes(p.stdout+p.stderr);rows.append({'label':label,'command':cmd,'exit':p.returncode});assert not p.returncode,(label,p.stderr.decode()[-2500:],p.stdout.decode()[-1000:])
base=old[1]['command'];base=base[:base.index('-c')];base.insert(1,'-I'+str(D))
with (O/'MESA_BUILD.lock').open('a')as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 for label,src in [('remesa_xcb','remesa_xcb.c')]:run(label+'-compile',base+['-c',str(D/src),'-o',str(D/(label+'.o'))])
 X=O/'gears-x11-batch-production-20261026T003000Z';Y=O/'gears-x11-bulk-clear-combined-20261026T030000Z'
 cmd=list(json.loads((Y/'COMPOSITION_BUILD_RESULTS.json').read_text())['command'])
 assert cmd.count(str(X/'remesa_xcb.o'))==1
 cmd=[str(D/'remesa_xcb.o')if x==str(X/'remesa_xcb.o')else x for x in cmd]
 cmd[cmd.index('-o')+1]=str(D/'libGL-gears-runtime.so.1');run('frontend-link',cmd)

# Mock regression includes all legacy cases, preserving LAB behavior.
run('test-i386-compile',base+['-static','-Wno-missing-prototypes','-Wl,--wrap=realloc','-Wl,--wrap=free',str(D/'test_staging.c'),str(D/'remesa_xcb.c'),str(W/'mesa/sgx535/remesa_present.c'),str(W/'mesa/sgx535/remesa_present_scale.c'),'-o',str(D/'test-staging-i386')])
Q=O/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';root=O/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root'
run('test-i386-run',[str(Q),str(D/'test-staging-i386')])
include=['-I'+str(W/'include'),'-I'+str(D),'-I'+str(W/'tools'),'-I'+str(W/'mesa/sgx535'),'-I'+str(M/'mesa-25.2.0/src'),'-I'+str(M/'mesa-25.2.0/include'),'-I'+str(M/'mesa-25.2.0/src/gallium/include'),'-I'+str(M/'build-i386/src'),'-I'+str(M/'build-i386/include')]
for mode,flags in [('native',[]),('ubsan',['-fsanitize=undefined','-fno-sanitize-recover=all'])]:
 binary=D/('test-staging-'+mode);run('test-'+mode+'-compile',['cc','-std=c11','-O2','-D_GNU_SOURCE','-DHAVE_STRUCT_TIMESPEC','-DHAVE_PTHREAD','-DHAVE_ENDIAN_H','-DHAVE_SECURE_GETENV','-DREMESA_WITH_MESA_HEADERS','-Wno-misleading-indentation',*include,*flags,'-Wl,--wrap=realloc','-Wl,--wrap=free',str(D/'test_staging.c'),str(D/'remesa_xcb.c'),str(W/'mesa/sgx535/remesa_present.c'),str(W/'mesa/sgx535/remesa_present_scale.c'),'-o',str(binary)],environment=dict(os.environ));run('test-'+mode+'-run',[str(binary)],environment=dict(os.environ))
result={'status':'OFFLINE_X11_PIPELINE_BUILD_MOCK_PASS_PHYSICAL_GAIN_UNPROVEN','GPU_submissions':0,'X_server_operations':0,'link_archive_base':str(N),'source_base':str(O/'gears-presentation-production-20261026T003000Z'),'commands':rows,'libGL_sha256':hashlib.sha256((D/'libGL-gears-runtime.so.1').read_bytes()).hexdigest(),'bootstrap_required':str(O/'gears-readback-target-production-20261026T003000Z/gears-bootstrap.so'),'production_transport_unchanged':True,'shader_geometry_kernel_unchanged':True,'MIT_SHM_enabled_by_this_change':False}
(D/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='commands'},indent=2))
