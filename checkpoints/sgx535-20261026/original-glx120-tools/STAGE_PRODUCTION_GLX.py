"""Stage only when explicitly run by the principal after fresh-boot CPU policy.
No GPU operation is issued by staging. Offline tests import helpers separately.
"""
from pathlib import Path
import subprocess,shlex,hashlib,json,io,tarfile,ast
D=Path(__file__).resolve().parent;O=D.parent;H=D/'hardware-glx';B=O/'gears-fast-reuse-qualified-clamp-20261025T214500Z';OLD=O/'gears-fast-blake3-20261025T205000Z'
REMOTE_ROOT='/root/sgx535-original-glx-less-single-20261026T162000Z'
REMOTE_BUNDLE='/root/sgx535-gears-less-single-native-20261026T162000Z/bundle'
REMOTE_CPU='/root/sgx535-color-vma-cpu-less-single-20261026T162000Z'
SUPERVISOR='/root/sgx535-less-single-supervisor-20261026T162000Z'
CLIENT_SHA='159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37'
def controller_source(manifest_sha,notes):
 original=(H/'SUPERVISOR.template.py').read_text();at=original.index('\n');cfg={'boot':'BOOT_PENDING','root':REMOTE_ROOT,'bundle':REMOTE_BUNDLE,'manifest_sha256':manifest_sha,'notes':notes}
 s='config='+repr(cfg)+original[at:]
 assert s.count("'BOOT_PENDING'")==1 and "'REMESA_GEARS_FRAME_LIMIT':'120'"in s
 assert 'GEARS_FRAME_GPU_RETIRED_READBACK'not in s and 'receipts.bin'not in s
 assert 'REMESA_GEARS_TRACE'not in s and 'REMESA_GEARS_DIAGNOSTIC'not in s
 assert "'REMESA_GEARS_READBACK':'MMAP'"in s and "'REMESA_GEARS_PRESENT':'PUTIMAGE'"in s
 assert 'seconds_since_FPS_progress'in s and 'for40seconds'in s
 ast.parse(s);return s

def main():
 H.mkdir(exist_ok=True);boot=(H/'OBSERVED_BOOT.txt').read_text().strip()
 cpu=json.loads((D/'LINUX_COLOR_PHYSICAL_RESULTS.json').read_text());assert cpu['boot']==boot and cpu['exit']==0 and cpu['GPU_submissions']==0 and cpu['legacy_capsule_unchanged'] and cpu['root']==REMOTE_CPU
 assert all(v=='N'for v in cpu['controls_after'].values())
 policies=json.loads(cpu['stdout']);assert policies['closed'] and policies['GEM_final_INFO_zero'] and policies['all_4096000_bytes_match_ioctl'] and policies['fork_existing_PTE_revocation_SIGBUS'] and policies['mprotect_write_exec_denied']
 qualified=json.loads((O/'gears-depth-less-single-clear-20261026T160400Z/hardware/PHYSICAL_SINGLE_CLEAR_RESULTS.json').read_text());assert qualified['status']=='PHYSICAL_SINGLE_CLEAR_COLOR_Z_COCOVERAGE_STABLE_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE' and qualified['boot']!=boot and len(qualified['frames'])==6
 assert all(row['phase']==9 and row['ledger']==7 and row['proof']==111 and row['Z24_never_farther_on_historical_visible_geometry'] and row['COLOR_silhouette_identical_to_ALWAYS'] and row['COLOR_Z_co_coverage_exact_all_pixels'] and row['analytical_stable_interior_pixels']>=100000 and row['analytical_visible_gear_class_exact_in_stable_interiors']for row in qualified['frames'])
 final_qualification_audit=json.loads((O/'gears-depth-less-single-clear-20261026T160400Z/hardware/FINAL_REMOTE_STATE.json').read_text());assert final_qualification_audit['status']=='ENDED_REPLAY6_NNN_STOCK_NO_PENDING_GPU_OPERATION' and final_qualification_audit['boot']==qualified['boot']
 notes={n:r['note_sha256']for n,r in json.loads((D/'INITRD_BUILD.json').read_text())['modules'].items()};assert cpu['notes']==notes
 files={f'assets/{n}':D/'stream-profile'/n for n in ['stream-profile.bin',*[f'role{i}.bin'for i in range(5)]]}
 expected={n:{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}for n,p in files.items()};payload=io.BytesIO()
 with tarfile.open(fileobj=payload,mode='w')as t:
  for n,p in files.items():data=p.read_bytes();info=tarfile.TarInfo(n);info.size=len(data);info.mode=0o600;t.addfile(info,io.BytesIO(data))
 original_sha=json.loads((O/'gears-x11-bulk-clear-combined-20261026T030000Z/hardware-glx/STAGED_NATIVE.json').read_text())['manifest_sha256'];scope={'status':'PRIVATE_GLX120_BUNDLE_STAGED_GPU0_NOT_GLX_APPROVAL','profile':'PRODUCTION_OPEN8_NOHASH_DEFINED_CHANNELS_Z24X8_NO_DIAGNOSTICS','maximum_frames':120,'readback':'FAST_NOHASH_OPEN8_PROOF111_MMAP_COLOR_FOR_X11','presentation':'PUTIMAGE','TRACE':False,'DIAGNOSTIC':False,'original_client_unchanged':True,'qualification_scope':'Physical typed LESS and single-triangle clear correction; stable analytical interiors, not full GL shading/precision conformance; original raw and failed prior tests preserved'}
 config={'boot':boot,'notes':notes,'cpu':cpu,'cpu_root':REMOTE_CPU,'expected':expected,'old_manifest_sha':original_sha,'old':'/root/sgx535-gears-bulk-clear-native-20261026T033000Z/bundle','new':REMOTE_BUNDLE,'scope':scope}
 code=(D/'stage_glx_bundle_helpers.py').read_text()+'\nconfig='+repr(config)+'\n'+'''import sys,tarfile,io,os
assert os.geteuid()==0 and os.uname().machine=='i686' and os.uname().release=='5.10.240-antix.1-486-smp'
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()==config['boot']
assert all(Path('/sys/module/gma500_gfx/parameters/'+n).read_text().strip()=='N'for n in ['remesa_memory_enabled','remesa_retained_lab_enabled','remesa_stream_enabled'])
for n,digest in config['notes'].items():assert file_hash('/sys/module/'+n+'/notes/.note.gnu.build-id')==digest
assert json.loads((Path(config['cpu_root'])/'RESULT.json').read_text())==config['cpu']
source=Path('/proc/sgx535_source_guard').read_text();assert 'producers_active=0\\n'in source and 'reasons=0\\n'in source and 'capsule_state=0\\n'in source
payload={}
with tarfile.open(fileobj=io.BytesIO(sys.stdin.buffer.read()))as t:
 for item in t.getmembers():
  assert item.isfile()and item.name in config['expected']and item.name not in payload
  assert item.size==config['expected'][item.name]['bytes'];payload[item.name]=t.extractfile(item).read()
assert set(payload)==set(config['expected'])
r=replace_bundle(config['old'],config['new'],payload,config['expected'],config['old_manifest_sha'],config['scope']);r['boot']=config['boot'];print(json.dumps(r))
'''
 ast.parse(code);(H/'STAGE_NATIVE_REMOTE.py').write_text(code)
 ssh=json.loads((O/'glx-physical-attempt-20261022T230000Z/SSH_ARGV.json').read_text());p=subprocess.run(ssh+['sudo -n python3 -c '+shlex.quote(code)],input=payload.getvalue(),capture_output=True,timeout=90);(H/'stage-native.stdout').write_bytes(p.stdout);(H/'stage-native.stderr').write_bytes(p.stderr);assert not p.returncode,p.stderr
 staged=json.loads(p.stdout);assert staged['GPU_submissions']==0 and staged['client_sha256']==CLIENT_SHA and staged['replaced_files']==expected
 s=controller_source(staged['manifest_sha256'],notes);(H/'LIVE_CONTROLLER.pending.py').write_text(s)
 s=(H/'LAUNCH_ONCE.py').read_text();ast.parse(s);(H/'LAUNCH_ONCE.py').write_text(s)
 staged.update(profile=scope,CPU_policy_prerequisite=str(D/'LINUX_COLOR_PHYSICAL_RESULTS.json'),freshboot_previously_GPU_unused=True,bundle=REMOTE_BUNDLE,root=REMOTE_ROOT,supervisor=SUPERVISOR)
 (H/'STAGED_NATIVE.json').write_text(json.dumps(staged,indent=2)+'\n');print('Private120frame bundle staged and all manifest files verified; GPU0; no controller launched')
if __name__=='__main__':main()
