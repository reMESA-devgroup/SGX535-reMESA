from pathlib import Path
import json,shutil,ast
D=Path(__file__).resolve().parent;O=D.parent;J=O/'gears-depth-less-background-plane-20261026T124000Z';B=J/'hardware';H=D/'hardware';H.mkdir()
assert json.loads((B/'FINAL_REMOTE_STATE.json').read_text())['status']=='ENDED_REPLAY6_NNN_STOCK_NO_PENDING_GPU_OPERATION'
assert json.loads((B/'LESS_BG_QUALIFICATION_FAILURE.json').read_text())['production_promotion']is False
assert json.loads((D/'COMPOSITION_RESULTS.json').read_text())['status']=='PASS_TYPED_SINGLE_CLEAR_COMPOSITION_OFFLINE_ONLY'
assert all(t['exit']==0 for t in json.loads((D/'NOHASH_TRUSTED_TEST_RESULTS.json').read_text())['tests'])
assert (D/'replay/LESS_AUDITOR_TEST_RESULTS.json').is_file()
old=json.loads((J/'INITRD_BUILD.json').read_text());new=json.loads((D/'INITRD_BUILD.json').read_text());prior=json.loads((B/'PREPARATION.json').read_text());boot=(B/'OBSERVED_BOOT.txt').read_text().strip();root='/root/sgx535-depth-less-single-boot-20261026T160400Z'
for n in ['prepare_boot.py','restore_boot.py','VERIFY_POSTBOOT.py','REBOOT_ONCE.py','STAGE_BOOT.py','BUILD_REPLAY_PACKAGE.py','LAUNCH_REPLAY_ONCE.py','RETRIEVE_REPLAY.py','FINAL_AUDIT.py']:
 s=(B/n).read_text().replace(prior['current_boot'],boot).replace(prior['root'],root).replace('530b515273b2615bb1717ec3801da8985277e2e3b58b95fd99a36760134d9e1e',old['modules']['gma500_gfx']['note_sha256'])
 s=s.replace('sgx535-depth-less-bg-','sgx535-depth-less-single-').replace('20261026T124000Z','20261026T160400Z').replace(old['image_sha256'],new['image_sha256']).replace(old['image_sha256'][:8],new['image_sha256'][:8]).replace(str(old['image_bytes']),str(new['image_bytes']))
 s=s.replace('PASS_BACKGROUND_REFERENCE_PLANE_COMPOSITION_OFFLINE_ONLY','PASS_TYPED_SINGLE_CLEAR_COMPOSITION_OFFLINE_ONLY')
 if n=='STAGE_BOOT.py':
  line="with (C/'depth1280-private-initrd.img').open('rb')as f:r=subprocess.run(cmd,stdin=f,capture_output=True,timeout=90)"
  assert line in s
  s=s.replace(line,"try:\n with (C/'depth1280-private-initrd.img').open('rb')as f:r=subprocess.run(cmd,stdin=f,capture_output=True,timeout=240)\nexcept subprocess.TimeoutExpired as e:\n (D/'stage-timeout.stdout').write_bytes(e.stdout or b'');(D/'stage-timeout.stderr').write_bytes(e.stderr or b'');(D/'STAGE_TIMEOUT.json').write_text(json.dumps({'status':'UNCERTAIN_REMOTE_STAGE_NO_RETRY','GPU_submissions':0,'reboots':0})+'\\n');print('Stage timeout; observe remote state, no retry/reboot');raise SystemExit(2)")
 ast.parse(s);(H/n).write_text(s)
(H/'PREPARATION.json').write_text(json.dumps({'current_boot':boot,'root':root,'new_image_sha256':new['image_sha256'],'new_entry':'sgx535-depth-less-single-'+new['image_sha256'][:8],'scope':'Original BG restored; exact one-triangle1280x800 Z-clear using existing typed TA index encoder, all12 LESS states intact','GPU_submissions':0},indent=2)+'\n')
s=(J/'RUN_LINUX_COLOR_CPU.py').read_text().replace('sgx535-color-vma-cpu-depth-less-bg-20261026T124000Z','sgx535-color-vma-cpu-depth-less-single-20261026T160400Z');(D/'RUN_LINUX_COLOR_CPU.py').write_text(s);shutil.copy2(J/'test-linux-color-i386',D/'test-linux-color-i386')
print('Single-clear one-shot helpers prepared; no remote action')
