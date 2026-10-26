from pathlib import Path
import json,struct,hashlib,collections,argparse,subprocess,errno
from defined_output_contract import verify_format_sources
from single_clear_output_contract import compare_single_clear_outputs
R=Path(__file__).resolve().parent;D=R.parent
p=argparse.ArgumentParser();p.add_argument('--actual',type=Path,default=R/'actual');p.add_argument('--allow-synthetic',action='store_true');p.add_argument('--expected-dir',type=Path,default=R/'expected');a=p.parse_args();A=a.actual;P=A/'capture';F=D/'stream-profile';E=a.expected_dir;h=lambda b:hashlib.sha256(b).digest()
result=json.loads((A/'RESULT.json').read_text());synthetic=result.get('synthetic',False)
format_contract=verify_format_sources(R/'DEFINED_CHANNEL_FORMAT_SOURCES.json')
old= D.parent/'gears-fast-blake3-20261025T205000Z/hardware/ended40/capture/receipts.bin'
old_blob=old.read_bytes();assert len(old_blob)==40*240;old_previous=bytes(32);old_sources=set()
for j in range(40):
 b=old_blob[j*240:(j+1)*240];session,seq,op,gen,proof,phase,ledger,error=struct.unpack_from('<4Q4I',b)
 assert session and seq==op==gen==j+1 and proof==255 and phase==9 and ledger==7 and not error and b[112:144]==old_previous
 assert h(b[:144]+bytes(32)+b[176:])==b[144:176];old_previous=b[144:176];old_sources.add(b[176:208])
assert len(old_sources)==40
# Bind every expected image and repeated input directly to historical raw
# readbacks AND the old B3/SHA receipt, not merely the40-receipt chain.
H=D.parent/'gears-fast-blake3-20261025T205000Z';HP=H/'hardware/ended40/capture'
historical_assets=[(H/'stream-profile'/f'role{i}.bin').read_bytes()for i in range(5)]
CI=D.parent/'gears-qualified-clamp-integration-20261025T213000Z'
base_profile=(CI/'stream-profile/stream-profile.bin').read_bytes();less_profile=(F/'stream-profile.bin').read_bytes()
assert len(base_profile)==len(less_profile) and less_profile[:16]==base_profile[:16] and less_profile[80:112]==base_profile[80:112] and less_profile[144:]==base_profile[144:]
assert less_profile[16:48].hex()==json.loads((D/'SINGLE_CLEAR_PROFILE_RESULTS.json').read_text())['profile_sha256'] and less_profile[48:208]==b''.join(h((F/f'role{j}.bin').read_bytes())for j in range(5))
assert all((F/f'role{i}.bin').read_bytes()==(CI/'stream-profile'/f'role{i}.bin').read_bytes()for i in(1,3,4)), 'Unchanged CI336 assets must remain exact'
assert all((F/f'role{i}.bin').read_bytes()==historical_assets[i]for i in(3,4)), 'Only CODE may differ from historical320 assets'
parent=D.parent/'gears-depth-less-correction-20261026T050000Z'
original_pds=(parent/'stream-profile/role0.bin').read_bytes();single_pds=(F/'role0.bin').read_bytes()
assert len(original_pds)==len(single_pds)and [j for j,(a,b)in enumerate(zip(original_pds,single_pds))if a!=b]==[0x42d6,0x42d7,0x42ea]
assert struct.unpack_from('<16f',single_pds,0x42c4)==(0,0,1,1,2560,0,1,1,0,1600,1,1,1280,800,1,1)
old_gen=(CI/'stream-profile/role2.bin').read_bytes();new_gen=(F/'role2.bin').read_bytes();states=json.loads((D.parent/'gears-frame-plan-20261025T032000Z/BATCHES.json').read_text());allowed={0x308}
for row in states:
 off=row['state_offset'];assert struct.unpack_from('<I',old_gen,off+4)[0]==0x01c00000 and struct.unpack_from('<I',new_gen,off+4)[0]==0x00400000
 allowed.update(range(off+4,off+8))
assert len(old_gen)==len(new_gen) and all(i in allowed for i,(a,b)in enumerate(zip(old_gen,new_gen))if a!=b)
assert len([i for i,(a,b)in enumerate(zip(old_gen,new_gen))if a!=b])==25
assert old_gen[0x308]==6 and new_gen[0x308]==3
old_use=historical_assets[1];new_use=(F/'role1.bin').read_bytes()
assert all(0x938<=j<0x950 or 0xd38<=j<0xd50 for j,(old_byte,new_byte)in enumerate(zip(old_use,new_use))if old_byte!=new_byte)
qualified=(D.parent/'stream-vertex-upper-clamp-oracle-20261025T184000Z/hardware/actual/capture/frame-000002-input-role1.bin').read_bytes()[0x938:0x948]
for name,offset in[('attribute',0x800),('state',0xc00)]:
 original=(CI/'cpu-output/native'/f'vs-{name}-original.bin').read_bytes()
 encoded=(CI/'cpu-output/native'/f'vs-{name}-clamped.bin').read_bytes()
 assert len(original)==320 and len(encoded)==336 and original==old_use[offset:offset+320] and encoded==new_use[offset:offset+336]
 assert encoded[:312]==original[:312] and encoded[312:328]==qualified and encoded[328:336]==original[312:320]
assert (CI/'candidate.c').read_bytes()==(D.parent/'gears-vertex-upper-clamp-20261025T182600Z/candidate.c').read_bytes()
contract=json.loads((CI/'CONTRACT_TEST_RESULTS.json').read_text());assert len(contract['tests'])==4 and all(row['exit']==0 for row in contract['tests'])

for pose in(1,2,3):
 old_receipt=old_blob[(pose-1)*240:pose*240];old_pds=(HP/f'frame-{pose:06d}-pds.bin').read_bytes()
 assert old_receipt[208:240]==h(b''.join(h(x)for x in[old_pds,*historical_assets[1:]]))
 for replay in(pose,pose+3):
  derived=(R/'updates'/f'frame-{replay:06d}-pds.bin').read_bytes();assert len(derived)==len(old_pds)and [j for j,(a,b)in enumerate(zip(old_pds,derived))if a!=b]==[0x42d6,0x42d7,0x42ea] and derived[0x42c4:0x4304]==single_pds[0x42c4:0x4304]
  for kind,off in[('color',48),('z',80)]:
   historical=HP/f'physical-{pose:06d}-{kind}.bin';raw=historical.read_bytes()
   digest=bytes.fromhex(subprocess.check_output([str(H/'b3-reference'),str(historical)],text=True).strip())
   assert digest==old_receipt[off:off+32]
   assert (E/f'frame-{replay:06d}-{kind}.bin').read_bytes()==raw, 'Expected replay oracle differs from authenticated historical raw bytes'

if synthetic:assert a.allow_synthetic,'Synthetic data never qualifies physical GPU execution'
assert result['exit']==0 and result['legacy_capsule_unchanged'] and all(v=='N'for v in result['controls_after'].values())
t=(F/'stream-profile.bin').read_bytes();assert struct.unpack_from('<4I',t)==(0x32505352,2,12,0)
assets=[(F/f'role{i}.bin').read_bytes()for i in range(5)];frames={i:[(R/'updates'/f'frame-{i:06d}-pds.bin').read_bytes(),*assets[1:]]for i in range(1,7)};receipts=[];rows=[]
for i in range(1,7):
 b=(P/f'frame-{i:06d}-receipt.bin').read_bytes();assert len(b)==240
 session,seq,op,gen,proof,phase,ledger,error=struct.unpack_from('<4Q4I',b);assert session and seq==op==gen==i and (proof,phase,ledger,error)==(111,9,7,0)
 assert b[48:]==bytes(192),'OPEN8 must not emit any cryptographic evidence';receipts.append(b)
 row={'frame':i,'operation':op,'phase':phase,'ledger':ledger,'proof':proof,'mode':'OPEN8 ABI5 NOHASH production kernel, separate raw qualification reader'}
 color=(P/f'frame-{i:06d}-color.bin').read_bytes();expected_color=(E/f'frame-{i:06d}-color.bin').read_bytes()
 depth=(P/f'frame-{i:06d}-z.bin').read_bytes();expected_depth=(E/f'frame-{i:06d}-z.bin').read_bytes()
 ref=D/'reference'/('pose'+str((i-1)%3+1))
 row.update(compare_single_clear_outputs(color,expected_color,depth,expected_depth,(ref/'CPU-DIAGNOSTIC-rgb.bin').read_bytes(),(ref/'CPU-DIAGNOSTIC-ndc-depth-f32.bin').read_bytes(),(ref/'CPU-DIAGNOSTIC-stable-interior-mask.bin').read_bytes()))
 rows.append(row)
assert len({r[:8]for r in receipts})==1
blob=(P/'transcript.bin').read_bytes();at=0;counts=collections.Counter();ids={};roles={};perrole=collections.Counter();reads={};coverage={};uploadcoverage={};finish=0;stages=0;infozero=False;opened=False;acknowledged=0;leases={};ta_denied=[];static_denied=[];pds_denied=0;negative_reads=[];closed=False;ta_reads=[];ta_syncs=[]
while at<len(blob):
 pair=[]
 for before in(0,1):
  magic,typ,which,ret,err,n=struct.unpack_from('<6I',blob,at);at+=24
  assert magic==0x32525354 and which==before and typ in(2,5) and n==(4152 if typ==2 else 656)
  if before==0:assert ret==err==0
  data=blob[at:at+n];assert len(data)==n;at+=n;pair.append((typ,data,ret,err))
 typ,before,_,_=pair[0];assert pair[1][0]==typ;_,after,ret,err=pair[1]
 version,act=struct.unpack_from('<2I',before);assert version==(1 if typ==2 else 5);counts[typ,act]+=1
 if typ==5:
  assert ret==err==0 and act in(8,2,3,4,5,6) and not leases
  if act==8:assert not opened;opened=True
  if act==3:stages+=1
  if act==5:acknowledged+=1
  if act==4:assert finish<6 and after[416:656]==receipts[finish];finish+=1
  if act==6:assert finish==acknowledged==6;closed=True
  continue
 ident=struct.unpack_from('<I',after,8)[0];offset=struct.unpack_from('<Q',before,40)[0];length=struct.unpack_from('<I',before,48)[0];flags=struct.unpack_from('<I',before,12)[0]
 if act==1:
  role=len(ids);assert role<11 and ident and ident not in roles;ids[role]=ident;roles[ident]=role
 role=roles.get(ident)
 if ret or err:
  assert opened and ret==0xffffffff and before==after
  if act==7:
   assert flags==1 and not any(before[56:]) and acknowledged==finish
   if role in(7,8):assert finish in(0,3);ta_denied.append((finish,role));assert err==errno.EPERM
   else:assert role in(1,2,3,4) and finish==1 and err==errno.EPERM;static_denied.append(role)
  else:
   assert act==9 and role==0 and finish==acknowledged==3 and leases.get(0)==1 and offset==0 and length==4096 and err==129 and pds_denied==0
   bad=before[56:4152];oldpage=frames[3][0][:4096];assert bad[0]==(oldpage[0]^1) and bad[1:]==oldpage[1:];pds_denied+=1
  continue
 if act in(1,2,3,4,5,6):assert role is not None;perrole[role,act]+=1
 if act==3:assert struct.unpack_from('<Q',before,32)[0]==struct.unpack_from('<Q',t,208+24*role+8)[0]
 if act==7:
  assert role is not None and role not in leases and (not flags or role<5);leases[role]=flags
 if act==10:assert role in leases;del leases[role]
 if act==9:
  assert role in range(5) and leases.get(role)==1 and 0<=finish<6 and length==4096 and offset%4096==0
  frame=finish+1;assert frame==1 or role==0;size=len(assets[role]);assert offset+length<=size
  key=frame,role;pages=uploadcoverage.setdefault(key,set());assert offset not in pages;pages.add(offset)
  assert before[56:4152]==frames[frame][role][offset:offset+4096]
 if act==11 and role in(7,8):
  assert role==8 and finish==acknowledged==5 and not leases and len(ta_reads)==2 and not ta_syncs
  ta_syncs.append((finish,role))
 if act==8 and role in(7,8):
  assert (finish,role)in((4,7),(5,8)) and acknowledged==finish and leases.get(role)==0 and offset==0 and length==4096
  assert len(ta_reads)<3
  if len(ta_reads)==2:assert ta_syncs==[(5,8)]
  ta_reads.append((finish,role,after[56:4152]));continue
 if act==8:
  assert role in(0,1,2,3,4,5,10) and 1<=finish<=6 and length==4096 and offset%4096==0 and role in leases
  if finish==acknowledged==3:
   assert role==0 and leases[0]==1 and offset==0;negative_reads.append(after[56:4152]);continue
  assert leases[role]==0 and acknowledged==finish-1
  size=len(assets[role])if role<5 else 4096000;assert offset+length<=size
  key=finish,role;pages=coverage.setdefault(key,set());assert offset not in pages;pages.add(offset);buf=reads.setdefault(key,bytearray(size));buf[offset:offset+length]=after[56:4152]
 if act==14:
  allocated,object_bytes,gpu_va,*stats=struct.unpack_from('<3Q10I',after,56)
  if ident:
   assert role is not None;va,size=struct.unpack_from('<QQ',t,208+24*role+8);pages=size//4096
   assert object_bytes==size and gpu_va==va and stats[6]==1 and stats[7]==pages and not stats[8] and stats[9]==pages and not stats[3] and not stats[4] and not stats[5]
  else:
   assert not allocated and not object_bytes and not gpu_va and not any(stats)
   if closed and counts[2,6]==11:infozero=True
assert at==len(blob) and finish==acknowledged==6 and stages==72 and len(ids)==11 and closed and infozero and not leases
assert ta_denied==[(0,7),(0,8),(3,7),(3,8)] and static_denied==[1,2,3,4] and pds_denied==1
assert [(frame,role)for frame,role,_ in ta_reads]==[(4,7),(5,8),(5,8)] and ta_syncs==[(5,8)]
assert ta_reads[1][2]==ta_reads[2][2], 'Lazy TA invalidation must match explicit full SYNC'
for (frame,role,data),name in zip(ta_reads,['ta-role7-after4-lazy.bin','ta-role8-after5-lazy.bin','ta-role8-after5-full-sync.bin']):assert len(data)==4096 and data==(P/name).read_bytes()
assert len(negative_reads)==2 and negative_reads[0]==negative_reads[1]==frames[3][0][:4096]
assert (P/'negative-pds-before.bin').read_bytes()==(P/'negative-pds-after.bin').read_bytes()==negative_reads[0]
dynamic_pages={struct.unpack_from('<Q',t,472+j*80+48)[0]//4096*4096 for j in range(12)}
for i in range(1,7):
 for role in range(5):
  expected=set(range(0,len(assets[role]),4096))if i==1 else(dynamic_pages if role==0 else set())
  assert uploadcoverage.get((i,role),set())==expected
 for role,kind in[(5,'color'),(10,'z'),*[(j,f'input-role{j}')for j in range(5)]]:
  data=reads[i,role];assert len(coverage[i,role])*4096==len(data) and bytes(data)==(P/f'frame-{i:06d}-{kind}.bin').read_bytes()
  if role<5:assert bytes(data)==frames[i][role]
assert all(perrole[role,act]==1 for role in range(11)for act in(1,2,3,4,5,6)) and counts[2,13]==1
assert counts[5,8]==counts[5,6]==1 and counts[5,1]==counts[5,7]==0 and counts[5,2]==counts[5,4]==counts[5,5]==6
report={'status':'CPU_SYNTHETIC_SINGLE_CLEAR_AUDITOR_PASS'if synthetic else'PHYSICAL_SINGLE_CLEAR_COLOR_Z_COCOVERAGE_STABLE_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE','boot':result['boot'],'TA_CPU_reads_after_GPU_before_next_frame':[(frame,role)for frame,role,_ in ta_reads],'TA_lazy_read_matches_explicit_full_SYNC':True,'frames':rows,'historical_precedent40_receipts_independently_chained':True,'scope':'Separate typed LESS/single-clear GPU qualification. Historical COLOR silhouette is exact; new COLOR/Z co-coverage and backgroundclear required on every pixel. Removed old depth-only artifacts require independent no-triangle reference. Stable foreground nearest-depth/gear-class checked. Preserved old Z-footprint qualifier FAILED; old raw/oracles unchanged. All72STAGE/6retirement9ledger7/5inputs/9negativegates/11BO/PTE/INFO0/TAcoherence retained. Not full GL shading/precision conformance','full_RAW_approval':False,'raw_Z_byte_exact':all(row['raw_Z_byte_exact']for row in rows),'format_contract_sources':format_contract,'FAST_is_not_LAB_approval':True,'new_glxgears_execution':False,'controls_after':result['controls_after'],'legacy_capsule_unchanged':True}
if synthetic:report.update(GPU_operations=0,scope='Synthetic auditor model with historical pixels, not new physical execution.')
(A.parent/('SYNTHETIC_SINGLE_CLEAR_RESULTS.json'if synthetic else'PHYSICAL_SINGLE_CLEAR_RESULTS.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
