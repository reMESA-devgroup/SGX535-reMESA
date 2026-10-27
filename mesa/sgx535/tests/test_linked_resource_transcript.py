#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Tamper tests using a retained probe transcript; never accesses hardware."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

repo=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('resource_audit',repo/'tools/remesa/verify_linked_program_resources.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
original=Path(sys.argv[1]).read_bytes();assert audit.verify(original)['final_INFO_zero_and_CLOSE']
stride=2*(24+4152)
calls=[(at,struct.unpack_from('<4I4Q2I',original,at+24))for at in range(0,len(original),stride)]
write=next(at for at,r in calls if r[1]==9)
info=next(at for at,r in calls if r[1]==14 and r[2])
depth=[at for at,r in calls if r[1]==3 and r[6]>=0x40001000][1]
unlease=next(at for at,r in calls if r[1]==10)
last_info=[at for at,r in calls if r[1]==14 and not r[2]][-1]
cases=[]
def rejected(name,data):
 try:audit.verify(data)
 except (AssertionError,KeyError,struct.error):cases.append(name)
 else:raise AssertionError('Accepted corrupted '+name)
rejected('truncated',original[:-1])
data=bytearray(original);struct.pack_into('<I',data,write+4176+8,1);rejected('failed_ioctl_header',data)
data=bytearray(original)
for payload in (write+24,write+4200):data[payload+56]^=1
rejected('write_changed_without_matching_readback',data)
data=bytearray(original)
for payload in (write+24,write+4200):data[payload+56+80]=1
rejected('reserved_capacity_filled',data)
data=bytearray(original)
for payload in (depth+24,depth+4200):struct.pack_into('<Q',data,payload+32,0x40001000)
rejected('overlapping_color_depth_VA',data)
data=bytearray(original);struct.pack_into('<I',data,info+4200+116,0);rejected('missing_verified_PTE',data)
rejected('missing_unlease',original[:unlease]+original[unlease+stride:])
data=bytearray(original);struct.pack_into('<Q',data,last_info+4200+56,4096);rejected('nonzero_final_allocation',data)
data=bytearray(original)
for payload in (write+24,write+4200):struct.pack_into('<Q',data,payload+16,0xffffffffffffffff)
rejected('foreign_context',data)
assert len(cases)==9
with tempfile.TemporaryDirectory() as tmp:
 output=Path(tmp)/'unapproved.json'
 result=subprocess.run([sys.executable,'-O',str(repo/'tools/remesa/verify_linked_program_resources.py'),
  '--transcript',sys.argv[1],'--output',str(output)],capture_output=True)
 assert result.returncode and not output.exists()
print('PASS 9 resource-transcript tamper cases plus optimized-Python refusal; CPU only, no new GPU or DRM operation')
