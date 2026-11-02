#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Separate gears LAB catalogue/receipt parsing. No device or rendering."""
import json,struct
from pathlib import Path
import private_probe as p
PROBE=struct.Struct('<IIQ18I2IQQ5Ii4I')
assert PROBE.size==152
def load_profile(catalogue,size,asset_root=None,*,homogeneous=False,clamped_api=False):
 c=json.loads(catalogue.read_text())
 if homogeneous and clamped_api:raise ValueError('ambiguous explicit catalogue mode')
 expected_format='REMESA_BOUND_GEARS_CLAMPED_API_V1' if clamped_api else 'REMESA_BOUND_GEARS_HOMOGENEOUS_V1' if homogeneous else 'REMESA_BOUND_GEARS_POSITIVE_V1'
 if c.get('format')!=expected_format or c.get('physical_qualification') is not False or c.get('GPU_operations')!=0:raise ValueError('wrong explicit gears catalogue mode')
 if homogeneous:
  from gears_homogeneous_oracle import VERTICES_SHA,COEFFICIENT_SHA
  if c.get('registered_kernel_profiles') is not False or [x['proposed_profile'] for x in c['profiles']]!=[3,4] or c['witness']['vertices_sha256']!=VERTICES_SHA or c['witness']['coefficient_sha256']!=COEFFICIENT_SHA or c['witness']['code_sha256']!='b60249a06a8fbed9f8d235a06cac8f7511cbafd9f1344a076a24535859d713fd':raise ValueError('changed homogeneous witness/selectors')
 if clamped_api:
  from build_gears_clamped_api_probe_profiles import WITNESS_SHA256
  if c.get('registered_kernel_profiles') is not False or [x['proposed_profile'] for x in c['profiles']]!=[5,6] or any(c['witness'][name+'_sha256']!=digest for name,digest in WITNESS_SHA256.items()):raise ValueError('changed actual-API witness/selectors')
 assert [x['size'] for x in c['profiles']]==['1280x800','640x640']
 index=next(i for i,x in enumerate(c['profiles']) if x['size']==size);profile=c['profiles'][index]
 def path(original,name):return asset_root/size/name if asset_root else Path(original)
 raw=path(profile['packet_path'],'packet.bin').read_bytes();placement=path(profile['placement_path'],'placement.bin').read_bytes()
 if len(raw)!=2264 or p.sha(raw)!=profile['packet_sha256'] or len(placement)!=776 or p.sha(placement)!=profile['placement_sha256']:raise ValueError('profile metadata changed')
 if struct.unpack_from('<I',raw,320)[0]!=2 or struct.unpack_from('<I',raw,396)[0]!=1:raise ValueError('depth finalization contract mismatch')
 if struct.unpack_from('<4I',raw,16)!=(1,18,11,2) or len(profile['bindings'])!=18:raise ValueError('unexpected closed profile shape')
 for field,name in [('vertices','vertices-source.bin'),('coefficient','coefficients-source.bin')]:
  source=(asset_root/size/name if asset_root else Path(profile['packet_path']).parent/name)
  if not source.exists() or p.sha(source.read_bytes())!=c['witness'][field+'_sha256']:raise ValueError('changed bound input source')
 bindings=[];images={}
 for i,b in enumerate(profile['bindings']):
  if (b['index']!=i or b['access'] not in (1,3) or b['cached']!=1 or b['gpu_va']&4095 or b['allocation_bytes']&4095 or not 0<b['allocation_bytes']<=33554432 or b['gpu_va']<0x20000000 or b['gpu_va']+b['allocation_bytes']>0xe0000000):raise ValueError('invalid closed placement')
  if struct.unpack_from('<QQII',placement,8+24*i)!=(b['gpu_va'],b['allocation_bytes'],b['captured_handle'],b['access']):raise ValueError('placement does not match catalogue')
  bindings.append({'index':i,'va':b['gpu_va'],'bytes':b['allocation_bytes'],'access':b['access']})
  if b['access']==1:
   data=path(b['path'],'bo-'+str(i)+'.bin').read_bytes()
   if len(data)!=b['allocation_bytes'] or p.sha(data)!=b['sha256']:raise ValueError('READ image changed')
   images[i]=data
 ids=[b['captured_handle'] for b in profile['bindings']]
 color,depth=struct.unpack_from('<II',raw,352);color=ids.index(color);depth=ids.index(depth)
 region=ids.index(struct.unpack_from('<I',raw,304)[0])
 if bindings[region]['access']!=3 or bindings[region]['bytes']>1048576:raise ValueError('REGION diagnostic mapping exceeds closed bound')
 guards=[]
 for lower,upper in [(0x20000000,0x28000000),(0x30000000,0x40000000),(0x40000000,0xe0000000)]:
  group=[b for b in bindings if lower<=b['va']<upper]
  if not group:continue
  for va in [min(b['va'] for b in group)-4096,max(b['va']+b['bytes'] for b in group)]:
   if lower<=va and va+4096<=upper and not any(va<b['va']+b['bytes'] and b['va']<va+4096 for b in bindings) and va not in guards:guards.append(va)
 if len(bindings)+len(guards)>32:raise ValueError('guard allocation exceeds endpoint capacity')
 return {'profile':index+(5 if clamped_api else 3 if homogeneous else 1),'size':size,'packet_sha256':p.sha(raw),'bindings':bindings,'color_index':color,'depth_index':depth,'region_index':region,'guard_va':guards},images

def receipt(packet,context,profile,ids,budget):
 q=PROBE.unpack(packet)
 if list(q[:23])!=[2,profile,context,*ids,budget,0] or any(q[33:]):raise ValueError('scene response changed request/reserved fields')
 r=dict(zip(['operation','cookie','core','revision','pds_base','phase','ledger','error','retained','stages'],q[23:33]))
 if (not r['operation'] or r['cookie']!=r['operation'] or r['core']!=0x01130000 or r['revision']!=0x10201 or r['pds_base']!=0x20000000 or r['phase']!=9 or r['ledger']!=7 or r['error'] or r['retained'] or r['stages']!=15):raise RuntimeError('incomplete scene lifecycle receipt: '+json.dumps(r))
 return r

