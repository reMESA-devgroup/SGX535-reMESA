#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Collector protocol/lifetime model only; synthetic small backing, GPU0."""
import tempfile,unittest,struct
from pathlib import Path
from unittest.mock import patch
import private_probe as p
import scene_probe as s
from test_private_probe import Transport
class SceneTransport(Transport):
 def ioctl(self,fd,number,packet,mutate):
  if number!=p.ioctl_number(11,s.PROBE.size):return super().ioctl(fd,number,packet,mutate)
  self.probed=True;self.calls.append(('scene',0));q=list(s.PROBE.unpack(packet));assert q[2]==self.context
  q[21:31]=[1,1,0x01130000,0x10201,0x20000000,9,7,0,0,15]
  fields={'cookie':22,'core':23,'revision':24,'pds_base':25,'phase':26,'ledger':27,'error':28,'retained':29,'stages':30,'reserved':31,'immutable':3}
  if self.fault in fields:q[fields[self.fault]]^=1
  if self.fault=='ioctl':raise OSError(5,'modeled uncertain execution')
  packet[:]=s.PROBE.pack(*q);return 0
class Client(unittest.TestCase):
 def test_success_and_uncertain_responses(self):
  for fault in [None,'cookie','core','revision','pds_base','phase','ledger','error','retained','stages','reserved','immutable','ioctl']:
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as d:
    output=Path(d);t=SceneTransport(fault)
    plan={'profile':1,'size':'1280x800','bindings':[{'index':i,'va':0x40001000+i*8192,'bytes':4096,'access':1 if i<12 else 3} for i in range(17)],'color_index':12,'depth_index':13,'region_index':14,'guard_va':[0x40000000,0x40024000]}
    images={i:bytes([i])*4096 for i in range(12)}
    with patch.object(p,'device_identity',return_value={'fixture':'CPU_MODEL'}),patch.object(p.os,'open',return_value=123),patch.object(p.os,'close') as closed,patch.object(p.fcntl,'ioctl',side_effect=t.ioctl):
     if fault:
      with self.assertRaises((RuntimeError,ValueError,OSError)):s.collect(Path('/never-device'),output,plan,images,1000)
     else:
      r=s.collect(Path('/never-device'),output,plan,images,1000,region_prefix=True);self.assertEqual(r['region_prefix']['bytes'],4096);self.assertEqual(r['region_prefix']['nonzero_bytes'],0);self.assertTrue((output/'region-prefix-readback.bin').exists());self.assertEqual(len(r['object_info']),19);self.assertEqual(r['status'],'RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING');self.assertFalse(r['hardware_driver_qualified']);self.assertEqual(r['memory_cleanup'],'complete');self.assertFalse(t.objects)
     closed.assert_called_once_with(123)
    if fault:self.assertFalse(any(action in [p.UNMAP,p.UNPIN,p.DESTROY,p.CLOSE] for action,rid in t.calls))
 def test_wire_size(self):self.assertEqual(s.PROBE.size,144)
if __name__=='__main__':unittest.main()
