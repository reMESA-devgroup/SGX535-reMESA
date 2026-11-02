#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Closed W=2 LAB expectation, reusing the retained physical sample-centre rule.
No device access or rendering; expectation bytes are never GPU input.
"""
import argparse,json,struct
from pathlib import Path
from fractions import Fraction as F
from gears_scene_oracle import sha,verify
VERTICES_SHA='dff9a40a7aa0eedb961a56cfc99e3afb8599fa34db80aeeb9b52af011fc3bf0f'
COEFFICIENT_SHA='249bb2192e3023180f356ed28a14638774b0d8804458c5c919c081952e34bd38'

def prepare(fixture,size,output):
 if size not in ['1280x800','640x640']:raise ValueError('unsupported closed target')
 width,height=map(int,size.split('x'));raw=(fixture/'vertices-source.bin').read_bytes();coeff=(fixture/'coefficients-source.bin').read_bytes()
 if sha(raw)!=VERTICES_SHA or sha(coeff)!=COEFFICIENT_SHA:raise ValueError('changed sealed W2 inputs')
 packet=(fixture/'packet.bin').read_bytes();placement=(fixture/'placement.bin').read_bytes()
 if len(packet)!=2264 or len(placement)!=776 or struct.unpack_from('<4I',packet,16)!=(1,18,11,2):raise ValueError('scene shape')
 if struct.unpack_from('<8I',packet,320)!=(2,80,width,height,width,width,4,0x3f800000):raise ValueError('target contract')
 ids=[struct.unpack_from('<I',packet,32+8*i)[0] for i in range(18)];color_id,depth_id=struct.unpack_from('<II',packet,352);color_index,depth_index=ids.index(color_id),ids.index(depth_id)
 for index in [color_index,depth_index]:
  va,n,rid,access=struct.unpack_from('<QQII',placement,8+24*index)
  if n!=width*height*4 or rid!=ids[index] or access!=3:raise ValueError('attachment extent')
 c=list(map(F,struct.unpack('<36f',coeff)));v=struct.unpack('<24f',raw);columns=[c[16+4*i:20+4*i] for i in range(4)]
 clip=[tuple(sum(F(v[8*i+t])*columns[t][k] for t in range(4)) for k in range(4)) for i in range(3)]
 if any(p[2]!=0 or p[3]!=2 for p in clip):raise ValueError('not the bounded homogeneous witness')
 window=[(F(width,2)*(p[0]/p[3]+1),F(height,2)*(1-p[1]/p[3])) for p in clip]
 color=bytearray(width*height*4);depth=bytearray(b'\xff\xff\xff\0'*(width*height));white=0
 # All vertices are inside this retained small footprint, bounded by input SHA.
 if any(not 0<=p[0]<48 or not 0<=p[1]<48 for p in window):raise ValueError('unexpected witness footprint')
 for y in range(48):
  for x in range(48):
   sample=(F(2*x+1,2),F(2*y+1,2));edges=[(b[0]-a[0])*(sample[1]-a[1])-(b[1]-a[1])*(sample[0]-a[0]) for a,b in zip(window,window[1:]+window[:1])]
   if any(e==0 for e in edges) and all(e>=0 for e in edges):raise ValueError('sample on edge')
   if all(e>0 for e in edges):
    white+=1;struct.pack_into('<I',color,4*(y*width+x),0xffffffff);struct.pack_into('<I',depth,4*(y*width+x),0x800000)
 if white!=(135 if width==1280 else 55):raise ValueError('changed rational witness coverage')
 output.mkdir(parents=True,exist_ok=False);(output/'COLOR.bin').write_bytes(color);(output/'Z24.bin').write_bytes(depth)
 result={'classification':'CPU_EXPECTATION_ONLY_NOT_GPU','GPU_operations':0,'physical_qualification':False,'size':size,'packet_sha256':sha(packet),'vertices_sha256':sha(raw),'coefficients_sha256':sha(coeff),'clip':[[str(v) for v in p] for p in clip],'window':[[str(v) for v in p] for p in window],'white_pixels':white,'color_index':color_index,'depth_index':depth_index,'background_color':0,'background_Z24':0xffffff,'foreground_Z24':0x800000,'depth_ignored_mask':0xff000000,'files':{'COLOR.bin':sha(color),'Z24.bin':sha(depth)},'scope':'Only the retained exact W2 witness; no generic clipping/interpolation or physical approval'}
 (output/'ORACLE.json').write_text(json.dumps(result,indent=2)+'\n');return result

def main():
 parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True);p=sub.add_parser('prepare');p.add_argument('--fixture',type=Path,required=True);p.add_argument('--size',choices=['1280x800','640x640'],required=True);p.add_argument('--output',type=Path,required=True);p=sub.add_parser('verify');p.add_argument('--oracle',type=Path,required=True);p.add_argument('--manifest-sha256',required=True);p.add_argument('--capture',type=Path,required=True);a=parser.parse_args()
 result=prepare(a.fixture,a.size,a.output) if a.command=='prepare' else verify(a.oracle,a.capture,a.manifest_sha256);print(json.dumps(result,indent=2))
 if a.command=='verify' and not result['pixel_match']:raise SystemExit(1)
if __name__=='__main__':main()
