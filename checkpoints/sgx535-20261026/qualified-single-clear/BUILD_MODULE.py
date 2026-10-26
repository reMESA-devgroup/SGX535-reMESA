from pathlib import Path
import json,subprocess,fcntl,shutil,importlib.util,hashlib
D=Path(__file__).resolve().parent;O=D.parent;F=O/'gears-frame-full-zclear-20261025T164700Z';cmd=json.loads((F/'CACHE_BUILD_RESULTS.json').read_text())['command'];cmd=[('M='+str(D/'module')if x.startswith('M=')else'KBUILD_EXTRA_SYMBOLS='+str(D/'observer/Module.symvers')if x.startswith('KBUILD_EXTRA_SYMBOLS=')else x)for x in cmd]
with(O/'KERNEL_BUILD.lock').open('a')as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 p=subprocess.run(cmd,capture_output=True);(D/'module-build.log').write_bytes(p.stdout+p.stderr);assert not p.returncode,p.stderr.decode()[-2500:]
spec=importlib.util.spec_from_file_location('verify',O/'gears-stream-module-cache-20261025T074000Z/verify_profile_elf.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v);r=v.verify(D)
(D/'KERNEL_BUILD.json').write_text(json.dumps({'command':cmd,'exit':0,'compiled_profile':r,'GPU_submissions':0},indent=2)+'\n')
for n in ['BUILD_INITRD.py','TEST_INIT_IDENTITY.py']:shutil.copyfile(F/n,D/n)
print('New private clamp/timing kernel CPU build + ELF profile match PASS; GPU0')
