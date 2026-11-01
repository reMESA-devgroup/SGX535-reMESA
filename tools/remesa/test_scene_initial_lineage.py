#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify preserved initialization DATA lineage; no GPU/device access."""
import argparse,hashlib,re,struct
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--physical-pds',type=Path,required=True);a=p.parse_args();w=Path(__file__).resolve().parents[2]
source=(w/'tools/psb-dri-re/frozen_kernel_initial.inc').read_bytes();header=(w/'include/remesa/scene_initial.h').read_text()
assert hashlib.sha256(source).hexdigest() in header
words=[0]*11;seen=set()
for role,offset,value in re.findall(r'\{(\d+), (0x[0-9a-f]+), (0x[0-9a-f]+)\}',source.decode()):
 offset=int(offset,16)
 if int(role)==0 and 0x4350<=offset<0x437c:
  assert offset not in seen and offset%4==0;seen.add(offset);words[(offset-0x4350)//4]=int(value,16)
original=struct.pack('<11I',*words);encoded=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',header.split('remesa_scene_initial_data[44]={',1)[1].split('};',1)[0]));physical=a.physical_pds.read_bytes()
assert encoded==original==physical[0x4350:0x437c]
assert words[0]==0x54c5 and words[1]==0x07f00100 and words[2]==0x02000000 and words[3:8]==[0]*5
print('PASS exact44-byte initialization DATA from unchanged sparse original and preserved physical PDS; GPU0, not a new physical qualification')
