from pathlib import Path
import tempfile,struct,json,subprocess,sys,errno
R=Path(__file__).resolve().parent;N=R.parent;F=Path(tempfile.mkdtemp(prefix='synthetic-single-clear-',dir=R));A=F/'actual';P=A/'capture';P.mkdir(parents=True);profile=(N/'stream-profile/stream-profile.bin').read_bytes();assets=[(N/'stream-profile'/f'role{i}.bin').read_bytes()for i in range(5)];frames={};receipts=[]
(A/'RESULT.json').write_text(json.dumps({'synthetic':True,'boot':'SYNTHETIC_GPU0','exit':0,'legacy_capsule_unchanged':True,'controls_after':{'memory':'N','retained':'N','stream':'N'}}))
for i in range(1,7):
 frames[i]=[(R/'updates'/f'frame-{i:06d}-pds.bin').read_bytes(),*assets[1:]];b=struct.pack('<4Q4I',123,i,i,i,111,9,7,0)+bytes(192);receipts.append(b);(P/f'frame-{i:06d}-receipt.bin').write_bytes(b)
 ref=N/'reference'/('pose'+str((i-1)%3+1));stable=(ref/'CPU-DIAGNOSTIC-stable-interior-mask.bin').read_bytes();refz=struct.unpack('<1024000f',(ref/'CPU-DIAGNOSTIC-ndc-depth-f32.bin').read_bytes());refrgb=(ref/'CPU-DIAGNOSTIC-rgb.bin').read_bytes()
 for kind in('color','z'):
  raw=bytearray((R/'expected'/f'frame-{i:06d}-{kind}.bin').read_bytes())
  for pix in range(1024000):
   if kind=='z':
    old=struct.unpack_from('<I',raw,pix*4)[0]&0xffffff
    oldcolor=(R/'expected'/f'frame-{i:06d}-color.bin').read_bytes() if pix==0 else oldcolor
    value=0xffffff if oldcolor[pix*4+3]==0 else(min(old,int((refz[pix]*.5+.5)*0xffffff))if stable[pix]else old)
    struct.pack_into('<I',raw,pix*4,value|0x5a000000)
   elif stable[pix]:
    red,green,blue=refrgb[pix*3:pix*3+3];raw[pix*4:pix*4+4]=bytes([blue,green,red,255])
  (P/f'frame-{i:06d}-{kind}.bin').write_bytes(raw)
 for role,data in enumerate(frames[i]):(P/f'frame-{i:06d}-input-role{role}.bin').write_bytes(data)
(P/'negative-pds-before.bin').write_bytes(frames[3][0][:4096]);(P/'negative-pds-after.bin').write_bytes(frames[3][0][:4096])
dynamic={struct.unpack_from('<Q',profile,472+i*80+48)[0]//4096*4096 for i in range(12)}
with(P/'transcript.bin').open('wb')as f:
 def pair(typ,a,b,error=0):
  for phase,data in enumerate((a,b)):f.write(struct.pack('<6I',0x32525354,typ,phase,0xffffffff if error and phase else 0,error if phase else 0,len(data)));f.write(data)
 def mem(act,role=None,offset=0,data=b'',flags=0,error=0):
  a=bytearray(4152);struct.pack_into('<4I',a,0,1,act,0 if role is None or act==1 else role+32,flags);struct.pack_into('<Q',a,16,7)
  if act==1:struct.pack_into('<Q',a,24,len(assets[role])if role<5 else struct.unpack_from('<Q',profile,208+24*role+16)[0])
  if act==3:struct.pack_into('<Q',a,32,struct.unpack_from('<Q',profile,208+24*role+8)[0])
  if act in(8,9):struct.pack_into('<Q',a,40,offset);struct.pack_into('<I',a,48,len(data))
  if act==9:a[56:56+len(data)]=data
  b=bytearray(a)
  if act==1:struct.pack_into('<I',b,8,role+32)
  if act==8:b[56:56+len(data)]=data
  if act==14 and role is not None:
   va,size=struct.unpack_from('<QQ',profile,208+24*role+8);pages=size//4096;struct.pack_into('<3Q10I',b,56,size,size,va,1,1,pages,0,0,0,1,pages,0,pages)
  pair(2,a,b,error)
 def stream(act,frame=0):
  a=bytearray(656);struct.pack_into('<2I',a,0,5,act);b=bytearray(a)
  if act==4:b[416:656]=receipts[frame-1]
  pair(5,a,b)
 mem(12);mem(14)
 for role in range(11):
  for act in(1,2,3):mem(act,role)
  mem(14,role)
 stream(8)
 for role in(7,8):mem(7,role,flags=1,error=errno.EPERM)
 for i in range(1,7):
  for role in(range(5)if i==1 else(0,)):
   mem(7,role,flags=1)
   for offset in(range(0,len(frames[i][role]),4096)if i==1 else sorted(dynamic)):mem(9,role,offset,frames[i][role][offset:offset+4096])
   mem(10,role);mem(11,role)
  stream(2)
  for j in range(12):stream(3)
  stream(4,i)
  for role,data in[(5,(P/f'frame-{i:06d}-color.bin').read_bytes()),(10,(P/f'frame-{i:06d}-z.bin').read_bytes()),*enumerate(frames[i])]:
   mem(7,role)
   for offset in range(0,len(data),4096):mem(8,role,offset,data[offset:offset+4096])
   mem(10,role)
  stream(5)
  if i in(4,5):
   role=7 if i==4 else 8;block=bytes([0xa0+role])*4096
   mem(7,role);mem(8,role,0,block);mem(10,role)
   (P/f'ta-role{role}-after{i}-lazy.bin').write_bytes(block)
   if i==5:
    mem(11,role);mem(7,role);mem(8,role,0,block);mem(10,role)
    (P/f'ta-role{role}-after{i}-full-sync.bin').write_bytes(block)
  if i==1:
   for role in range(1,5):mem(7,role,flags=1,error=errno.EPERM)
  if i==3:
   for role in(7,8):mem(7,role,flags=1,error=errno.EPERM)
   raw=frames[3][0][:4096];bad=bytes([raw[0]^1])+raw[1:];mem(7,0,flags=1);mem(8,0,0,raw);mem(9,0,0,bad,error=129);mem(8,0,0,raw);mem(10,0)
 stream(6)
 for role in reversed(range(11)):
  for act in(4,5,6):mem(act,role)
 mem(14);mem(13)
def run(allow=True,extra=()):return subprocess.run([sys.executable,str(R/'VERIFY_SINGLE_CLEAR_OUTPUTS.py'),'--actual',str(A),*extra,*(['--allow-synthetic']if allow else[])],capture_output=True)
p=run();(F/'positive.stdout').write_bytes(p.stdout);(F/'positive.stderr').write_bytes(p.stderr);assert not p.returncode,p.stderr.decode();positive=json.loads((F/'SYNTHETIC_SINGLE_CLEAR_RESULTS.json').read_text());assert positive['raw_Z_byte_exact']is False and all(x['Z24_never_farther_on_historical_visible_geometry'] and x['COLOR_silhouette_identical_to_ALWAYS'] and x['COLOR_Z_co_coverage_exact_all_pixels'] and x['Z24_strictly_closer_pixels']>=100 and x['analytical_stable_interior_pixels']>=100000 for x in positive['frames']);assert run(False).returncode
neg=[]
for path,index in[(P/'frame-000002-receipt.bin',48),(P/'frame-000003-input-role1.bin',0),(P/'frame-000001-color.bin',0),(P/'frame-000001-color.bin',3),(P/'frame-000001-z.bin',0),(P/'negative-pds-after.bin',0),(P/'ta-role7-after4-lazy.bin',0),(P/'ta-role8-after5-full-sync.bin',0)]:
 raw=path.read_bytes();bad=bytearray(raw);bad[index]^=1;path.write_bytes(bad);assert run().returncode,path;path.write_bytes(raw);neg.append(path.name)
Bad=F/'bad-expected';Bad.mkdir()
for i in range(1,7):
 for kind in('color','z'):
  dst=Bad/f'frame-{i:06d}-{kind}.bin';dst.symlink_to(R/'expected'/dst.name)
bad=Bad/'frame-000001-color.bin';raw=bad.read_bytes();bad.unlink();corrupt=bytearray(raw);corrupt[0]^=1;bad.write_bytes(corrupt);assert run(extra=('--expected-dir',str(Bad))).returncode
assert not run().returncode
(R/'SINGLE_CLEAR_AUDITOR_TEST_RESULTS.json').write_text(json.dumps({'status':'CPU_SYNTHETIC_SINGLE_CLEAR_AUDITOR_PASS','GPU_operations':0,'fixture':str(F),'LESS_depth_and_gear_class_fixture_accepted_with_raw_FALSE':True,'positive_FINISH':6,'positive_STAGE':72,'TA_EPERR_refusals':4,'static_EPERR_refusals':4,'PDS_EKEYREJECTED_with_unchanged_page':1,'tamper_rejected':neg+['owned expected oracle'],'synthetic_flag_mandatory':True,'scope':'Fabricated journal and counters, synthetic nearest-depth stable interiors over historical coverage; no new GPU proof'},indent=2)+'\n');print('PASS synthetic DEFINED_CHANNELS6/72,4TA+4static+1PDSnegatives,receipt/input/COLOR/PDStransaction/expected tamper;GPU0')
