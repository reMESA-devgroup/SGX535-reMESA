#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Analytical LAB oracle for the finite window-coordinate white triangle.
Only compares GPU readbacks. Does not render, present or supply GPU data.
Checks all stable pixel centres and defined Z24 (one quantization unit); X8 is
undefined. Exact edge ties require COLOR/Z co-coverage but allow either fill.
"""
import argparse,array,json,struct,sys
from pathlib import Path

def verify(capture):
 c=json.loads((capture/'RESULT.json').read_text());w,h=map(int,c['plan']['size'].split('x'));pitch=(w+31)&~31
 assert c['status']=='RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING' and c['memory_cleanup']=='complete' and c['guards_intact']
 packet=(capture.parent/c['plan']['size']/'packet.bin').read_bytes();clear_bits=struct.unpack_from('<I',packet,348)[0];clear=struct.unpack('<f',struct.pack('<I',clear_bits))[0]
 assert clear in (1,.25)
 # Fixed existing producer exports screen positions, unit N/L, and Z=.125.
 raw=(capture.parent/c['plan']['size']/'bo-6.bin').read_bytes()
 expected=[(w*.25,h*.25),(w*.75,h*.25),(w*.25,h*.75)]
 for i,(x,y) in enumerate(expected):assert struct.unpack_from('<12f',raw,i*48)==(x,y,.125,1,0,0,1,0,0,0,1,0)
 arrays=[]
 for name in ('color_index','depth_index'):
  data=(capture/('bo-'+str(c['plan'][name])+'-readback.bin')).read_bytes();assert len(data)>=pitch*h*4
  a=array.array('I');a.frombytes(data)
  if sys.byteorder!='little':a.byteswap()
  arrays.append(a)
 color,depth=arrays;foreground_z=round(.125*0xffffff);background_z=round(clear*0xffffff)
 x0,y0=w//2,h//2;dx,dy=w,h # doubled-coordinate legs
 fg=bg=edge=bad_color=bad_depth=0;examples=[]
 for y in range(h):
  for x in range(w):
   px,py=2*x+1-x0,2*y+1-y0;s=dy*px+dx*py-dx*dy
   inside=px>0 and py>0 and s<0;boundary=px>=0 and py>=0 and s==0
   index=y*pitch+x;cv=color[index];z=depth[index]&0xffffff
   if inside:fg+=1;wanted=0xffffffff;wanted_z=foreground_z
   elif boundary:edge+=1;wanted=cv if cv in (0,0xffffffff) else -1;wanted_z=foreground_z if cv==0xffffffff else background_z
   else:bg+=1;wanted=0;wanted_z=background_z
   wrong_color=cv!=wanted;wrong_z=abs(z-wanted_z)>(0 if wanted_z==0xffffff else 1)
   bad_color+=wrong_color;bad_depth+=wrong_z
   if (wrong_color or wrong_z) and len(examples)<12:examples.append({'x':x,'y':y,'color':hex(cv),'Z24':hex(z),'expected_color':hex(wanted),'expected_Z24':hex(wanted_z)})
 return {'status':'PHYSICAL_FINITE_TRIANGLE_COLOR_Z24_PASS' if not bad_color and not bad_depth else 'PHYSICAL_FINITE_TRIANGLE_COLOR_Z24_FAIL','width':w,'height':h,'stable_foreground_pixels':fg,'stable_background_pixels':bg,'exact_edge_tie_pixels':edge,'COLOR_mismatches':bad_color,'Z24_mismatches':bad_depth,'examples':examples,'X8_undefined_ignored':True,'foreground_Z24_quantization_tolerance':1,'scope':'Analytical coverage and white unit-Lambert value for fixed finite inputs, definedZ24/co-coverage; not full raw equality, API conformance, sustained rendering or FPS'}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=verify(a.capture);a.output.open('x').write(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));sys.exit(0 if r['status'].endswith('_PASS') else 1)
