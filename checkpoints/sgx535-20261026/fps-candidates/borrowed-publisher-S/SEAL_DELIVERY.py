from pathlib import Path
import json,hashlib,struct,difflib
D=Path(__file__).resolve().parent;O=D.parent;S=O/'gears-depth-less-single-clear-20261026T160400Z';B=O/'gears-fps-helper-borrowed-publisher-20261026T131000Z';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs=json.loads((D/'COMPOSITION_INPUTS.json').read_text());allowed={'module/'+n for n in inputs['allowed_source_changes']};same=[];changed=[]
for rel,h in inputs['base_S_inputs'].items():
 assert sha(S/rel)==h,'OriginalSchanged:'+rel
 if rel in allowed:
  assert (D/rel).read_bytes()==(B/rel).read_bytes()and sha(D/rel)!=h;changed.append(rel);(D/(Path(rel).name+'.patch')).write_text(''.join(difflib.unified_diff((S/rel).read_text().splitlines(True),(D/rel).read_text().splitlines(True),fromfile=str(S/rel),tofile=str(D/rel))))
 else:assert sha(D/rel)==h,rel;same.append(rel)
assert set(changed)==allowed
assert (D/'module/Makefile').read_bytes()==(S/'module/Makefile').read_bytes()
assert sha(D/'module/remesa_canonical_inputs.h')==inputs['S_canonical_header_sha256']
assert sha(D/'module/stream_profile_asset.h')==inputs['S_profile_header_sha256']
assert sha(S/'hardware/PHYSICAL_SINGLE_CLEAR_RESULTS.json')==inputs['base_S_physical_gate_sha256']
for r in json.loads((D/'S_GOLDEN_INPUT_IDENTITIES.json').read_text())['frames']:
 for k in('color','z'):assert sha(r[k]['path'])==r[k]['sha256']
reg=json.loads((D/'REGRESSION_SUITE_RESULTS.json').read_text());assert reg['status']=='PASS'and len(reg['tests'])==9 and all(t['exit']==0 for t in reg['tests'])
helpers=json.loads((D/'PUBLISHER_VIEWS_TEST_RESULTS.json').read_text());assert len(helpers['tests'])==3 and all(t['exit']==0 for t in helpers['tests'])
profile=json.loads((D/'MODULE_PROFILE_VERIFICATION.json').read_text());assert profile['status']=='COMPILED_ELF_PROFILE_BYTE_EXACT_PASS'and profile['symbol_bytes']==5844 and profile['module_sha256']==sha(D/'module/gma500_gfx.ko')
p=(D/'stream-profile/stream-profile.bin').read_bytes();assert p==(S/'stream-profile/stream-profile.bin').read_bytes()and p[16:48].hex()==profile['profile_sha256']
init=json.loads((D/'INITRD_BUILD.json').read_text());assert init['modules']['gma500_gfx']['sha256']==profile['module_sha256']and not init['installed']and init['GPU_submissions']==0
assert json.loads((D/'INITRD_OFFLINE_SUITE_RESULTS.json').read_text())['status']=='PASS_OFFLINE_ONLY_NOT_STAGED'
identity=json.loads((D/'INIT_CHAIN_TEST_RESULTS.json').read_text());assert identity['classification']=='CPU_ONLY_IDENTITY_CHAIN_TEST_PASS'and len(identity['cases'])==5 and all(c['pass']for c in identity['cases'])and not identity['deployed']and not identity['booted']
pinned=json.loads((O/'handoff-resume-20261024T071200Z/PROTECTED.json').read_text())['expected_sha256']
for path,h in pinned.items():assert sha(path)==h,path
scope={'status':'SEALED_SINGLE_CLEAR_BORROWED_PUBLISHER_OFFLINE_PHYSICAL_PENDING','GPU_operations':0,'SSH_calls':0,'X_server_operations':0,'base_S':str(S),'qualified_S_pixel_gate':inputs['base_S_physical_gate'],'unchanged_S_files':same,'changed_sources':{rel:sha(D/rel)for rel in sorted(changed)},'all_S_profile_assets_canonical_headers_byte_exact':True,'copied_K_profile_data':False,'profile_binary_path':str(D/'stream-profile/stream-profile.bin'),'profile_digest':profile['profile_sha256'],'compiled_profile_symbol_bytes':5844,'compiled_profile_sha256':profile['actual_compiled_profile_sha256'],'kernel_module_sha256':profile['module_sha256'],'kernel_note_sha256':init['modules']['gma500_gfx']['note_sha256'],'image_path':str(D/'depth1280-private-initrd.img'),'image_sha256':init['image_sha256'],'image_bytes':init['image_bytes'],'initrd_identity_offline_pass':True,'staged':False,'booted':False,'protected_previous_files':len(pinned),'new_helper_test_modes':['native','UBSan','i386/qemu'],'existing_regression_scripts':reg['tests'],'expected_physical_comparison':'Full COLOR exactS; Z24 exactS with highX8VOID; all6retirement/ACK/9negatives/input/journal/BO/PTE gates retained','physical_gain_demonstrated':False};(D/'COMPOSITION_RESULTS.json').write_text(json.dumps(scope,indent=2)+'\n')
files=[q for q in D.iterdir()if q.is_file()and q.name!='DELIVERY_MANIFEST.json'];(D/'DELIVERY_MANIFEST.json').write_text(json.dumps({'status':scope['status'],'profile_digest':profile['profile_sha256'],'kernel_module_sha256':profile['module_sha256'],'kernel_note_sha256':scope['kernel_note_sha256'],'image_sha256':init['image_sha256'],'image_bytes':init['image_bytes'],'files':{q.name:{'bytes':q.stat().st_size,'sha256':sha(q)}for q in sorted(files)},'staged':False,'physical_gain_demonstrated':False},indent=2)+'\n');print(json.dumps({k:v for k,v in scope.items()if k in ['status','profile_digest','kernel_module_sha256','kernel_note_sha256','image_sha256','image_bytes']},indent=2))
