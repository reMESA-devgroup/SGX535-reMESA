#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Separate state15/W2 catalogue generator; never modifies the old profiles."""
import argparse,copy,json,struct
from pathlib import Path
from build_gears_scene_probe_profiles import build as build_common,checked
from gears_homogeneous_oracle import VERTICES_SHA,COEFFICIENT_SHA

def build(c):
 if c.get('format')!='REMESA_BOUND_GEARS_HOMOGENEOUS_V1' or c.get('registered_kernel_profiles') is not False:raise ValueError('not an unregistered homogeneous candidate')
 witness=c['witness']
 if witness['code_sha256']!='b60249a06a8fbed9f8d235a06cac8f7511cbafd9f1344a076a24535859d713fd':raise ValueError('not the preserved CODE320')
 if witness['vertices_sha256']!=VERTICES_SHA or witness['coefficient_sha256']!=COEFFICIENT_SHA:raise ValueError('not the sealed W2 witness')
 if [p['proposed_profile'] for p in c['profiles']]!=[3,4]:raise ValueError('explicit homogeneous selectors required')
 normalized=copy.deepcopy(c);normalized['format']='REMESA_BOUND_GEARS_POSITIVE_V1'
 # Shared checks authenticate every owned image, overlap, packet/placement,
 # CODE320, dense coefficient source and raw PN packing before serialization.
 text=build_common(normalized)
 for p in c['profiles']:
  w,h=map(int,p['size'].split('x'));packet=checked(p['packet_path'],p['packet_sha256'],2264);images={b['captured_handle']:(b,checked(b['path'],b['sha256'],b['allocation_bytes'])) for b in p['bindings'] if b['access']==1}
  states=[(b,data) for b,data in images.values() if struct.unpack_from('<I',data)[0]==0x1f41]
  if len(states)!=1:raise ValueError('one homogeneous draw state required')
  b,data=states[0];va=b['gpu_va']
  if struct.unpack_from('<I',data,56)[0] or data[60:64]!=bytes(4):raise ValueError('state15 final mode/padding')
  if data[20:44]!=struct.pack('<6f',w*.5,w*.5,h*.5,-h*.5,.5,.5):raise ValueError('not the closed full viewport')
  if data[64:80]!=struct.pack('<4I',0xa0000000,0x28a1e001,0xa0200780,0xfb274000):raise ValueError('state15 MOVE/tail')
  if struct.unpack_from('<II',data,96)!=(va,0x81c0000e):raise ValueError('state15 PDS source/count')
  use=[struct.unpack_from('<6I',packet,400+24*i) for i in range(11)]
  if len([u for u in use if u[0]==b['captured_handle'] and u[1:3]==(64,16) and u[4]==0])!=1:raise ValueError('state15 auxiliary ownership')
  ta_id,ta_offset,ta_bytes=struct.unpack_from('<3I',packet,360);ta=images[ta_id][1]
  if ta_bytes!=104 or struct.unpack_from('<I',ta,ta_offset+56)[0]!=(0x40000000|((va+96)>>4)):raise ValueError('state15 TA reference')
 text=text.replace('REMESA_GEARS_SCENE_PROBE_PROFILES_H','REMESA_GEARS_HOMOGENEOUS_PROBE_PROFILES_H').replace('"remesa_scene_probe_profiles.h"','"remesa_gears_scene_probe_profiles.h"')
 text='\n'.join(line for line in text.split('\n') if not line.startswith('struct remesa_gears_scene_probe_profile {'))
 return text.replace('gears_scene_probe_packet_','gears_homogeneous_probe_packet_').replace(' gears_scene_probe_profiles[2]',' gears_homogeneous_probe_profiles[2]').replace('tools/remesa/build_gears_scene_probe_profiles.py','tools/remesa/build_gears_homogeneous_probe_profiles.py')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--catalogue',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();text=build(json.loads(a.catalogue.read_text()))
 with a.output.open('x') as f:f.write(text)
 print('Generated separate state15 profiles3/4, no physical qualification')
if __name__=='__main__':main()
