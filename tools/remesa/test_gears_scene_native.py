#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bound Gallium collector faults with actual C syscall-model records; GPU0."""
import argparse,json,shutil,struct,subprocess,tempfile,unittest
import gears_scene_trace as trace
from pathlib import Path
from unittest.mock import patch
import gears_scene_native as native
import gears_scene_profile as scene
import private_probe as p
parser=argparse.ArgumentParser();parser.add_argument('--model-output',type=Path,required=True);parser.add_argument('--catalogue',type=Path,required=True);a=parser.parse_args()

class Collector(unittest.TestCase):
 def test_lifetime_and_failures(self):
  for size in ['1280x800','640x640']:
   source=a.model_output/(size+'--execute');raw=(source/'INVENTORY-REMAINING.bin').read_bytes()
   objects=[struct.unpack_from('<QQII',raw,16+i*24) for i in range(7)];byid={x[2]:x for x in objects}
   profile=next(f for f in json.loads(a.catalogue.read_text())['profiles'] if f['size']==size);fixture=Path(profile['packet_path']).parent
   for fault in [None,'child','receipt','read','guard','remaining','packet','trace','trace_owner','missing_response','timeout']:
    with self.subTest(size=size,fault=fault),tempfile.TemporaryDirectory() as temp:
     output=Path(temp);calls=[];plan,images=scene.load_profile(a.catalogue,size)
     class Memory:
      def __init__(self,fd,context):assert fd==99 and context==77;self.poisoned=False
      def info(self,rid=0):
       calls.append(('INFO',rid));va,n,_,_=byid[rid] if rid else (0,0,0,0)
       return (sum(x[1] for x in objects),n,va,7,7,sum(x[1]//4096 for x in objects),0,0,0,int(bool(rid)),n//4096,0,n//4096)
      def request(self,action,rid=0):assert action in (p.UNMAP,p.UNPIN,p.DESTROY,p.CLOSE);calls.append((action,rid))
     def child(argv,**kwargs):
      assert kwargs['pass_fds']==(99,) and argv[5]=='--execute'
      assert argv[6:]==[] and plan['profile'] in [1,2]
      for f in source.iterdir():
       if f.is_file():shutil.copyfile(f,output/f.name)
      if fault=='timeout':raise subprocess.TimeoutExpired(argv,180)
      if fault=='missing_response':(output/'RESPONSE.bin').unlink()
      if fault=='trace_owner':
       path=output/'TRACE-REQUEST.bin';b=bytearray(path.read_bytes());struct.pack_into('<Q',b,8,78);path.write_bytes(b)
      if fault=='receipt':
       path=output/'RESPONSE.bin';b=bytearray(path.read_bytes());struct.pack_into('<I',b,124,8);path.write_bytes(b)
      if fault=='read':
       path=output/('after-id-'+str(struct.unpack_from('<I',(output/'placement.bin').read_bytes(),24)[0])+'.bin');b=bytearray(path.read_bytes());b[0]^=1;path.write_bytes(b)
      if fault=='guard':
       rid=struct.unpack_from('<I',(output/'GUARD_IDS.bin').read_bytes())[0];(output/f'after-id-{rid}.bin').write_bytes(bytes(4096))
      if fault=='remaining':
       path=output/'INVENTORY-REMAINING.bin';b=bytearray(path.read_bytes());struct.pack_into('<I',b,32,999);path.write_bytes(b)
      if fault=='trace':
       path=output/'TRACE-RESPONSE.bin';b=bytearray(path.read_bytes());struct.pack_into('<I',b,4272,999);path.write_bytes(b)
      if fault=='packet':
       path=output/'packet.bin';b=bytearray(path.read_bytes());b[396]^=1;path.write_bytes(b)
      return subprocess.CompletedProcess(argv,1 if fault=='child' else 0)
     client=a.model_output/'model'
     with patch.object(p,'device_identity',return_value={'scope':'MODEL_GPU0'}),patch.object(native.os,'open',return_value=99),patch.object(native.os,'close') as close,patch.object(native.subprocess,'run',side_effect=child),patch.object(p,'Memory',Memory):
      if fault:
       with self.assertRaises((RuntimeError,ValueError,FileNotFoundError,subprocess.TimeoutExpired)):native.collect(Path('/never-device'),output,plan,images,client,fixture,p.sha(client.read_bytes()))
       self.assertFalse(any(isinstance(action,int) for action,_ in calls))
      else:
       result=native.collect(Path('/never-device'),output,plan,images,client,fixture,p.sha(client.read_bytes()))
       self.assertEqual(result['memory_cleanup'],'complete');self.assertEqual(result['status'],'RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING');self.assertEqual(len(result['object_info']),7)
       self.assertEqual(len([1 for action,_ in calls if action==p.DESTROY]),7);self.assertIn((p.CLOSE,0),calls);self.assertTrue(result['guards_intact']);self.assertFalse(result['reference_images_uploaded_by_collector'])
      close.assert_called_once_with(99)
 def test_complete_trace_and_receipt_rejection(self):
  for size in ['1280x800','640x640']:
   source=a.model_output/(size+'--execute');plan,_=scene.load_profile(a.catalogue,size)
   raw=(source/'RESPONSE.bin').read_bytes();q=scene.PROBE.unpack(raw);ids=list(q[3:21]);context=q[2]
   receipt=scene.receipt(raw,context,plan['profile'],ids,1000)
   blob=(source/'TRACE-RESPONSE.bin').read_bytes()[16:]
   decoded=trace.decode(blob,context,receipt,plan)
   self.assertEqual(len(decoded['bindings']),18)
   for at in [0,4,8,12,16,20,24,28,40,44,48,52,60,64,72,80]:
    with self.subTest(size=size,trace_offset=at):
     bad=bytearray(blob);bad[at]^=1
     with self.assertRaises(ValueError):trace.decode(bad,context,receipt,plan)
   for at in [0,4,8,16,20,84,88,92,100,108,112,116,120,124,128,132,136,140,144,148]:
    with self.subTest(size=size,receipt_offset=at):
     bad=bytearray(raw);bad[at]^=1
     with self.assertRaises((ValueError,RuntimeError)):scene.receipt(bad,context,plan['profile'],ids,1000)
   for length in [0,127,4256,4279,4281]:
    with self.assertRaises(ValueError):trace.decode(blob[:length] if length<len(blob) else blob+b'\0',context,receipt,plan)
if __name__=='__main__':unittest.main(argv=['test_gears_scene_native.py'])
