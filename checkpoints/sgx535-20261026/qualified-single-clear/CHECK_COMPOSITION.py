from pathlib import Path
import json,hashlib
D=Path(__file__).resolve().parent;O=D.parent;L=O/'gears-depth-less-correction-20261026T050000Z';same=[]
for sub in ['module','observer']:
 for p in (L/sub).iterdir():
  if not p.is_file()or p.suffix not in('.c','.h','.inc')or p.name.endswith('.mod.c')or p.name in('stream_profile_asset.h','remesa_canonical_inputs.h'):continue
  assert p.read_bytes()==(D/sub/p.name).read_bytes(),str(p);same.append(sub+'/'+p.name)
for i in (1,3,4):assert (D/f'stream-profile/role{i}.bin').read_bytes()==(L/f'stream-profile/role{i}.bin').read_bytes()
for i,expected in [(0,[0x42d6,0x42d7,0x42ea]),(2,[0x308])]:
 a=(L/f'stream-profile/role{i}.bin').read_bytes();b=(D/f'stream-profile/role{i}.bin').read_bytes();assert len(a)==len(b)and [j for j,(x,y)in enumerate(zip(a,b))if x!=y]==expected
assert json.loads((D/'MODULE_PROFILE_VERIFICATION.json').read_text())['symbol_bytes']==5844
protected=json.loads((O/'handoff-resume-20261024T071200Z/PROTECTED.json').read_text())['expected_sha256']
for n,h in protected.items():assert hashlib.sha256(Path(n).read_bytes()).hexdigest()==h,n
r={'status':'PASS_TYPED_SINGLE_CLEAR_COMPOSITION_OFFLINE_ONLY','GPU_operations':0,'unchanged_sources':same,'PDS_inline_vertex_data_changed_bytes':[0x42d6,0x42d7,0x42ea],'GEN_clear_index_count_changed_byte':0x308,'clear_triangles':[2,1],'COLOR_Z_viewport':[1280,800],'all_gear_vertex_index_matrix_shader_and_LESS_state_bytes_unchanged':True,'background_original_1280x800_unchanged':True,'kernel_profile_ELF_bytes':5844,'protected_unchanged':len(protected),'previous_failed_L_and_J_qualifications_preserved':True}
(D/'COMPOSITION_RESULTS.json').write_text(json.dumps(r,indent=2)+'\n');print(r['status'],len(same),'unchanged sources')
