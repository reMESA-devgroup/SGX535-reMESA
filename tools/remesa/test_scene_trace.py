#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Metadata protocol model only; synthetic bytes, no GPU execution."""
import struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import scene_trace as t
import scene_probe as s
import private_probe as p
from test_scene_probe import SceneTransport

def fixture(context,plan,ids=None):
 ids=ids or list(range(32,49));h=[1,4256,1,15,9,7,0,0,2,3,17,29,31,20,24,0,context,1,1,123,1000,2000,3000,4000]
 b=bytearray(4256);t.HEADER.pack_into(b,0,*h)
 t.EVENT.pack_into(b,1800,4,1,0x2000,0,0,5,2100)
 t.EVENT.pack_into(b,1832,4,1,4,0x10,3,8,4100)
 for i,x in enumerate(plan['bindings']):t.BINDING.pack_into(b,3848+i*24,ids[i],x['access'],x['va'],x['bytes'])
 return b

def plan():return {'profile':1,'size':'1280x800','bindings':[{'index':i,'va':0x40001000+i*8192,'bytes':4096,'access':1 if i<12 else 3} for i in range(17)],'color_index':12,'depth_index':13,'region_index':14,'guard_va':[0x40000000,0x40024000]}
R={'operation':1,'cookie':1,'phase':9,'ledger':7,'stages':15,'error':0,'retained':0}
class TraceTransport(SceneTransport):
 def ioctl(self,fd,number,packet,mutate):
  if number!=p.ioctl_number(12,t.QUERY.size):return super().ioctl(fd,number,packet,mutate)
  self.calls.append(('trace',0));version,reserved,context,raw=t.QUERY.unpack(packet)
  assert version==1 and not reserved and context==self.context and not any(raw)
  raw=fixture(context,plan(),list(self.objects)[:17])
  if self.fault=='trace':raw[1800+64]=1 # invalid unused event
  packet[:]=t.QUERY.pack(1,0,context,raw);return 0
class Codec(unittest.TestCase):
 def test_valid_and_malformed(self):
  v=fixture(77,plan());q=t.decode(v,77,R,plan());self.assertEqual(q['event_count'],2);self.assertEqual(q['events'][1]['ns'],4100)
  for off,value in [(0,2),(4,0),(8,3),(12,0),(16,0),(20,0),(24,1),(28,1),(32,65),(40,18),(44,0),(48,0),(52,0),(56,1),(60,1),(64,78),(72,2),(80,2),(1864,1),(3848,0),(3852,3),(3856,0),(3864,0)]:
   with self.subTest(offset=off):
    b=bytearray(v);struct.pack_into('<I',b,off,value)
    with self.assertRaises(ValueError):t.decode(b,77,R,plan())
  with self.assertRaises(ValueError):t.decode(v[:-1],77,R,plan())
 def test_collector_and_trace_failure_cleanup(self):
  for fault in [None,'trace']:
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as d:
    transport=TraceTransport(fault);images={i:bytes([i])*4096 for i in range(12)}
    with patch.object(p,'device_identity',return_value={'fixture':'CPU_MODEL'}),patch.object(p.os,'open',return_value=123),patch.object(p.os,'close') as closed,patch.object(p.fcntl,'ioctl',side_effect=transport.ioctl):
     if fault:
      with self.assertRaises(ValueError):s.collect(Path('/never-device'),Path(d),plan(),images,1000,trace=True)
     else:
      result=s.collect(Path('/never-device'),Path(d),plan(),images,1000,trace=True,region_full=True);self.assertTrue((Path(d)/'region-full-readback.bin').exists());self.assertEqual(result['region_readback']['bytes'],4096);self.assertEqual(result['trace_ioctl_calls'],1);self.assertEqual(result['trace']['event_count'],2)
     self.assertFalse(transport.objects);self.assertIn((p.CLOSE,0),transport.calls);closed.assert_called_once_with(123)
if __name__=='__main__':unittest.main()
