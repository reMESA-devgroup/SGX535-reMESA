from pathlib import Path
import json,hashlib,subprocess,shlex
D=Path(__file__).resolve().parent;H=D/'hardware';O=D.parent;boot=(H/'OBSERVED_BOOT.txt').read_text().strip();notes={n:i['note_sha256']for n,i in json.loads((D/'INITRD_BUILD.json').read_text())['modules'].items()};binary=(D/'test-linux-color-i386').read_bytes();sha=hashlib.sha256(binary).hexdigest();ssh=json.loads((O/'glx-physical-attempt-20261022T230000Z/SSH_ARGV.json').read_text())
code='''from pathlib import Path
import subprocess,sys,os,json,hashlib,struct
boot=BOOT;notes=NOTES;sha=SHA
assert os.geteuid()==0 and Path('/proc/sys/kernel/random/boot_id').read_text().strip()==boot
assert os.uname().release=='5.10.240-antix.1-486-smp' and os.uname().machine=='i686'
for n,h in notes.items():assert hashlib.sha256(Path('/sys/module/'+n+'/notes/.note.gnu.build-id').read_bytes()).hexdigest()==h
names=['remesa_memory_enabled','remesa_retained_lab_enabled','remesa_stream_enabled'];base=Path('/sys/module/gma500_gfx/parameters')
assert all((base/n).read_text().strip()=='N'for n in names)
assert Path('/sys/class/drm/card0/device/vendor').read_text().strip()=='0x8086' and Path('/sys/class/drm/card0/device/device').read_text().strip()=='0x8108'
root=Path('/root/sgx535-color-vma-cpu-depth-less-single-20261026T160400Z');root.mkdir(mode=0o700)
data=sys.stdin.buffer.read();assert hashlib.sha256(data).hexdigest()==sha
client=root/'test-linux-color-i386';client.write_bytes(data);client.chmod(0o700)
cap=Path('/proc/sgx535_current_operation').read_bytes();assert len(cap)==4152 and struct.unpack_from('<11I',cap,8)==(1,4152,0,0,0,0,0,0,0,0,0)
source=Path('/proc/sgx535_source_guard').read_text();assert 'producers_active=0\\n'in source and 'released_reset=0\\n'in source
(root/'capsule-before.bin').write_bytes(cap);(root/'source-before.raw').write_text(source)
(root/'dmesg.before').write_bytes(subprocess.check_output(['dmesg']))
(root/'ATTEMPT_CONSUMED').write_text('one CPU-only GEM/VMA policy attempt; GPU submissions0\\n')
result={'classification':'REAL_LINUX_GEM_READONLY_MMAP_CPU_POLICY_TEST_NOT_GPU_RENDERING','GPU_submissions':0,'boot':boot,'notes':notes,'client_sha256':sha,'root':str(root)}
try:
 (base/'remesa_memory_enabled').write_text('Y');assert (base/'remesa_memory_enabled').read_text().strip()=='Y'
 p=subprocess.run([str(client),'--run'],capture_output=True,timeout=50);(root/'stdout').write_bytes(p.stdout);(root/'stderr').write_bytes(p.stderr);result.update(exit=p.returncode,stdout=p.stdout.decode(),stderr=p.stderr.decode())
except BaseException as e:
 result['controller_error']=repr(e)
finally:
 (base/'remesa_memory_enabled').write_text('N')
 result['controls_after']={n:(base/n).read_text().strip()for n in names}
 result['legacy_capsule_unchanged']=Path('/proc/sgx535_current_operation').read_bytes()==cap
 result['source_after']=Path('/proc/sgx535_source_guard').read_text()
 (root/'dmesg.after').write_bytes(subprocess.check_output(['dmesg']))
 (root/'RESULT.json').write_text(json.dumps(result,indent=2)+'\\n')
 print(json.dumps(result),flush=True)
assert result.get('exit')==0 and all(x=='N'for x in result['controls_after'].values()) and result['legacy_capsule_unchanged']
assert 'producers_active=0\\n'in result['source_after'] and 'reasons=0\\n'in result['source_after']
'''.replace('BOOT',repr(boot)).replace('NOTES',repr(notes)).replace('SHA',repr(sha))
p=subprocess.run(ssh+['sudo -n python3 -c '+shlex.quote(code)],input=binary,capture_output=True,timeout=65);(H/'linux-color-cpu.stdout').write_bytes(p.stdout);(H/'linux-color-cpu.stderr').write_bytes(p.stderr);print(p.returncode,p.stdout.decode(),p.stderr.decode());assert not p.returncode
r=json.loads(p.stdout);(D/'LINUX_COLOR_PHYSICAL_RESULTS.json').write_text(json.dumps(r,indent=2)+'\n')
