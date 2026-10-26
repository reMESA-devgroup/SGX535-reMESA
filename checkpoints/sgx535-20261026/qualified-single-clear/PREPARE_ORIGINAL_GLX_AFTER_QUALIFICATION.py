"""Prepare exact original GLXGears experiment ONLY after J physical pixels pass.
No network, reboot, X11 or GPU actions. Keep X11 performance candidate separate.
"""
from pathlib import Path
import json,ast,shutil,hashlib
J=Path(__file__).resolve().parent;O=J.parent;Y=O/'gears-x11-bulk-clear-combined-20261026T030000Z';B=Y/'hardware-glx';Q=J/'hardware'
r=json.loads((Q/'PHYSICAL_SINGLE_CLEAR_RESULTS.json').read_text())
assert r['status']=='PHYSICAL_SINGLE_CLEAR_COLOR_Z_COCOVERAGE_STABLE_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE'
assert len(r['frames'])==6 and all(v['phase']==9 and v['ledger']==7 and v['proof']==111 and v['Z24_never_farther_on_historical_visible_geometry'] and v['COLOR_silhouette_identical_to_ALWAYS'] and v['COLOR_Z_co_coverage_exact_all_pixels'] and v['analytical_stable_interior_pixels']>=100000 and v['analytical_visible_gear_class_exact_in_stable_interiors']for v in r['frames'])
assert json.loads((Q/'FINAL_REMOTE_STATE.json').read_text())['status']=='ENDED_REPLAY6_NNN_STOCK_NO_PENDING_GPU_OPERATION'
D=O/'gears-less-single-original-glx-20261026T162000Z';D.mkdir();H=D/'hardware-glx';H.mkdir()
for n in ['INITRD_BUILD.json','test-linux-color-i386']:shutil.copy2(J/n,D/n)
for n in ['production_glx_contract.py','VERIFY_PRODUCTION_GLX.py','libGL-gears-runtime.so.1']:shutil.copy2(Y/n,D/n)
assert hashlib.sha256((D/'libGL-gears-runtime.so.1').read_bytes()).hexdigest()==json.loads((Y/'COMPOSITION_BUILD_RESULTS.json').read_text())['libGL_sha256']
(D/'stream-profile').symlink_to(J/'stream-profile',target_is_directory=True)
boot=(Q/'OBSERVED_BOOT.txt').read_text().strip();newid=json.loads((J/'INITRD_BUILD.json').read_text())['image_sha256'][:8]
oldroot='/root/sgx535-depth-less-single-boot-20261026T160400Z';newroot='/root/sgx535-less-single-glx-boot-20261026T162000Z'
for n in ['prepare_boot.py','restore_boot.py','VERIFY_POSTBOOT.py','REBOOT_ONCE.py']:
 s=(Q/n).read_text().replace(json.loads((Q/'PREPARATION.json').read_text())['current_boot'],boot).replace(oldroot,newroot)
 # Current admitted kernel is now J, rather than the previous L module.
 s=s.replace('6a742a3fe80ab279602a3a47efdd346b74a93ddd678035e4526d11713ac028e7',json.loads((J/'INITRD_BUILD.json').read_text())['modules']['gma500_gfx']['note_sha256'])
 ast.parse(s);(H/n).write_text(s)
(H/'PREPARATION.json').write_text(json.dumps({'current_boot':boot,'root':newroot,'new_image_sha256':json.loads((J/'INITRD_BUILD.json').read_text())['image_sha256'],'new_entry':'sgx535-depth-less-single-'+newid,'image_reused_exact_not_rewritten':True,'scope':'Original1280x800 GLXGears, J corrected profile, exact qualified YlibGL/Rbootstrap; no X11 FPS candidate mixed','GPU_submissions':0},indent=2)+'\n')
replacements={'/root/sgx535-original-glx-bulk-clear-20261026T033000Z':'/root/sgx535-original-glx-less-single-20261026T162000Z','/root/sgx535-gears-bulk-clear-native-20261026T033000Z/bundle':'/root/sgx535-gears-less-single-native-20261026T162000Z/bundle','/root/sgx535-bulk-clear-supervisor-20261026T033000Z':'/root/sgx535-less-single-supervisor-20261026T162000Z','/root/sgx535-color-vma-cpu-bulk-clear-20261026T033000Z':'/root/sgx535-color-vma-cpu-less-single-20261026T162000Z'}
for n in ['SUPERVISOR.template.py','LAUNCH_ONCE.py','RETRIEVE_GLX_ENDED.py','FINAL_GLX_AUDIT.py','READ_HEALTH.py']:
 s=(B/n).read_text()
 for a,b in replacements.items():s=s.replace(a,b)
 s=s.replace('sgx535-gears-bulk-clear-native-20261026T033000Z','sgx535-gears-less-single-native-20261026T162000Z');ast.parse(s);(H/n).write_text(s)
for n in ['RUN_LINUX_COLOR_CPU.py','READ_SSH_BOOT.py','VERIFY_PRODUCTION_GLX.py']:
 s=(Y/n).read_text()
 for a,b in replacements.items():s=s.replace(a,b)
 ast.parse(s);(D/n).write_text(s)
s=(Y/'stage_glx_bundle_helpers.py').read_text().replace("REPLACEMENTS={'variant/libGL.so.1'}","REPLACEMENTS={'assets/stream-profile.bin',*[f'assets/role{i}.bin'for i in range(5)]}");(D/'stage_glx_bundle_helpers.py').write_text(s)
s=(Y/'STAGE_PRODUCTION_GLX.py').read_text()
for a,b in replacements.items():s=s.replace(a,b)
a=s.index(' qualified=json.loads(');b=s.index(' notes=',a)
s=s[:a]+''' qualified=json.loads((O/'gears-depth-less-single-clear-20261026T160400Z/hardware/PHYSICAL_SINGLE_CLEAR_RESULTS.json').read_text());assert qualified['status']=='PHYSICAL_SINGLE_CLEAR_COLOR_Z_COCOVERAGE_STABLE_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE' and qualified['boot']!=boot and len(qualified['frames'])==6
 assert all(row['phase']==9 and row['ledger']==7 and row['proof']==111 and row['Z24_never_farther_on_historical_visible_geometry'] and row['COLOR_silhouette_identical_to_ALWAYS'] and row['COLOR_Z_co_coverage_exact_all_pixels'] and row['analytical_stable_interior_pixels']>=100000 and row['analytical_visible_gear_class_exact_in_stable_interiors']for row in qualified['frames'])
 final_qualification_audit=json.loads((O/'gears-depth-less-single-clear-20261026T160400Z/hardware/FINAL_REMOTE_STATE.json').read_text());assert final_qualification_audit['status']=='ENDED_REPLAY6_NNN_STOCK_NO_PENDING_GPU_OPERATION' and final_qualification_audit['boot']==qualified['boot']
'''+s[b:]
s=s.replace("files={'variant/libGL.so.1':D/'libGL-gears-runtime.so.1'}","files={f'assets/{n}':D/'stream-profile'/n for n in ['stream-profile.bin',*[f'role{i}.bin'for i in range(5)]]}")
s=s.replace("(O/'gears-readback-target-production-20261026T003000Z/hardware-glx/STAGED_NATIVE.json')","(O/'gears-x11-bulk-clear-combined-20261026T030000Z/hardware-glx/STAGED_NATIVE.json')").replace("'old':'/root/sgx535-gears-direct-target-native-20261026T030000Z/bundle'","'old':'/root/sgx535-gears-bulk-clear-native-20261026T033000Z/bundle'")
s=s.replace('Defined COLOR+Z24 only; X8 VOID raw mismatch preserved, not fullRAW/LAB approval','Physical typed LESS and single-triangle clear correction; stable analytical interiors, not full GL shading/precision conformance; original raw and failed prior tests preserved')
ast.parse(s);(D/'STAGE_PRODUCTION_GLX.py').write_text(s)
(D/'OFFLINE_PREPARATION.json').write_text(json.dumps({'GPU_submissions':0,'scope':'Original GLXGears1280x800, only6asset replacements in complete verified Ybundle; no renderer/library/bootstrap/client changes','qualified_profile':str(J),'required_original_client_sha256':'159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37','new_X11_optimization_included':False,'CPU_readback_for_presentation_only':True,'per_frame_hash_checkpoint_oracle_diagnostics':False},indent=2)+'\n');print('Original GLXGears experiment prepared, no remote action:',D)
