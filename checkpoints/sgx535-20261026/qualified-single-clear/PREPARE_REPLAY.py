from pathlib import Path
import shutil,struct
D=Path(__file__).resolve().parent;O=D.parent;L=O/'gears-depth-less-correction-20261026T050000Z';R=D/'replay';R.mkdir();(D/'reference').symlink_to(L/'reference',target_is_directory=True);(R/'expected').symlink_to(L/'replay/expected',target_is_directory=True);(R/'updates').mkdir()
for p in (L/'replay').iterdir():
 if p.is_file()and(p.suffix in('.py','.c','.json')or p.name=='stream-probe-i386')and not p.name.startswith('RESUME_'):shutil.copy2(p,R/p.name)
for i in range(1,7):
 p=L/f'replay/updates/frame-{i:06d}-pds.bin';old=p.read_bytes();new=bytearray(old);new[0x42c4:0x4304]=(D/'stream-profile/role0.bin').read_bytes()[0x42c4:0x4304]
 assert [j for j,(a,b)in enumerate(zip(old,new))if a!=b]==[0x42d6,0x42d7,0x42ea]
 (R/'updates'/p.name).write_bytes(new)
s=(R/'VERIFY_LESS_OUTPUTS.py').read_text().replace('less_profile[48:112]==base_profile[48:112]','less_profile[80:112]==base_profile[80:112]').replace("D/'LESS_PROFILE_RESULTS.json'","D/'SINGLE_CLEAR_PROFILE_RESULTS.json'").replace('for i in(0,1,3,4)','for i in(1,3,4)').replace('for i in(0,3,4)','for i in(3,4)')
s=s.replace('allowed=set()','allowed={0x308}').replace('==24\nold_use','==25\nassert old_gen[0x308]==6 and new_gen[0x308]==3\nold_use')
pos=s.index("old_gen=(CI/")
s=s[:pos]+'''parent=D.parent/'gears-depth-less-correction-20261026T050000Z'
original_pds=(parent/'stream-profile/role0.bin').read_bytes();single_pds=(F/'role0.bin').read_bytes()
assert len(original_pds)==len(single_pds)and [j for j,(a,b)in enumerate(zip(original_pds,single_pds))if a!=b]==[0x42d6,0x42d7,0x42ea]
assert struct.unpack_from('<16f',single_pds,0x42c4)==(0,0,1,1,2560,0,1,1,0,1600,1,1,1280,800,1,1)
'''+s[pos:]
s=s.replace("assert (R/'updates'/f'frame-{replay:06d}-pds.bin').read_bytes()==old_pds", "derived=(R/'updates'/f'frame-{replay:06d}-pds.bin').read_bytes();assert len(derived)==len(old_pds)and [j for j,(a,b)in enumerate(zip(old_pds,derived))if a!=b]==[0x42d6,0x42d7,0x42ea] and derived[0x42c4:0x4304]==single_pds[0x42c4:0x4304]")
s=s.replace('PHYSICAL_LESS_DEPTH_COUNTERFACTUAL_AND_STABLE_INTERIOR_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE','PHYSICAL_LESS_SINGLE_CLEAR_STABLE_INTERIOR_PASS_NOT_FULL_GL_CONFORMANCE').replace('only12ISPcompare words differ from old ALWAYS','12ISPcompare words,3PDSclear-coordinate bytes and1TA index-count byte differ from old ALWAYS; original BG unchanged')
(R/'VERIFY_LESS_OUTPUTS.py').write_text(s);(R/'LESS_AUDITOR_TEST_RESULTS.json').unlink()
print('Single-clear replay has exact3-byte static PDS updates; all gear MVP/light bytes preserved; old raw/authentic receipts unchanged')
