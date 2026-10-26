from pathlib import Path
import json,array,struct,zlib,math
D=Path(__file__).resolve().parent;rows=[]
def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v)&0xffffffff)
for k in range(1,7):
 p=D/'hardware/replay-ended6/capture';c=(p/f'frame-{k:06d}-color.bin').read_bytes();z=array.array('I');z.frombytes((p/f'frame-{k:06d}-z.bin').read_bytes());oldc=(D/f'replay/expected/frame-{k:06d}-color.bin').read_bytes();oz=array.array('I');oz.frombytes((D/f'replay/expected/frame-{k:06d}-z.bin').read_bytes());r=D/f'reference/pose{(k-1)%3+1}';mask=(r/'CPU-DIAGNOSTIC-stable-interior-mask.bin').read_bytes();ref=array.array('f');ref.frombytes((r/'CPU-DIAGNOSTIC-ndc-depth-f32.bin').read_bytes());rgb=(r/'CPU-DIAGNOSTIC-rgb.bin').read_bytes()
 stats={'frame':k,'closer':0,'farther':0,'coverage_changed':0,'missing_COLOR_on_depth':0,'missing_COLOR_stable':0,'wrong_gear_stable':0,'depth_wrong_stable':0,'first_failures':[]}
 for i in range(len(z)):
  a=z[i]&0xffffff;b=oz[i]&0xffffff
  stats['closer']+=a<b-2;stats['farther']+=a>b+2;stats['coverage_changed']+=(a==0xffffff)!=(b==0xffffff)
  if a<0xffffff and c[i*4+3]==0:
   stats['missing_COLOR_on_depth']+=1;stats['missing_COLOR_stable']+=bool(mask[i])
   if len(stats['first_failures'])<5:stats['first_failures'].append({'pixel':i,'stable':mask[i],'old_COLOR':oldc[i*4:i*4+4].hex(),'new_COLOR':c[i*4:i*4+4].hex(),'old_Z':b,'new_Z':a})
  if mask[i]:
   stats['depth_wrong_stable']+=abs(a/0xffffff-(ref[i]*.5+.5))>1e-4
   channels=[c[i*4+2],c[i*4+1],c[i*4]];want=rgb[i*3:i*3+3];stats['wrong_gear_stable']+=max(channels)==0 or max(range(3),key=lambda j:channels[j])!=max(range(3),key=lambda j:want[j])
 rows.append(stats)
 if k<=3:
  im=bytearray()
  for y in range(800):
   im.append(0)
   for x in range(1280):
    i=(y*1280+x)*4;im.extend((c[i+2],c[i+1],c[i]))
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',1280,800,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(im,6))+chunk(b'IEND',b'');(D/f'PHYSICAL-LESS-frame-{k:06d}.png').write_bytes(png)
(D/'PHYSICAL_LESS_DIAGNOSTIC.json').write_text(json.dumps({'scope':'Diagnostic only, does not approve failed qualification','frames':rows},indent=2)+'\n');print(json.dumps(rows,indent=2))
