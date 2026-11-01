#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Collector/lifetime model, using real C constructor syscall-model records.
No device opened, no GPU commands, no pixels rendered or presented.
"""
import argparse,json,shutil,struct,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import private_probe as p
import scene_probe as scene
import scene_native as native
parser=argparse.ArgumentParser();parser.add_argument('--model-output',type=Path,required=True);parser.add_argument('--catalogue',type=Path,required=True);args=parser.parse_args()
class Client(unittest.TestCase):
 def test_retirement_collection_and_fail_closed(self):
  for size in ['1280x800','640x640']:
   source=args.model_output/size;metadata=(source/'placement.bin').read_bytes();bindings=[struct.unpack_from('<QQII',metadata,8+24*i) for i in range(17)]
   gids=struct.unpack('<4I',(source/'GUARD_IDS.bin').read_bytes());gvas=struct.unpack('<4Q',(source/'GUARD_VA.bin').read_bytes());extras=list(struct.iter_unpack('<QQII',(source/'EXTRAS.bin').read_bytes()))
   objects=bindings+[(va,4096,rid,0) for rid,va in zip(gids,gvas)]+extras;byid={b[2]:b for b in objects}
   for fault in [None,'child','receipt','guards','extras']:
    with self.subTest(size=size,fault=fault),tempfile.TemporaryDirectory() as temp:
     output=Path(temp);calls=[];plan,images=scene.load_profile(args.catalogue,size)
     class Memory:
      def __init__(self,fd,context):self.poisoned=False;self.ids=set();self.context=context;assert fd==99 and context==77
      def info(self,rid=0):
       calls.append(('info',rid));n=byid[rid][1] if rid else 0;va=byid[rid][0] if rid else 0
       return (sum(b[1] for b in objects),n,va,23,23,sum(b[1]//4096 for b in objects),0,0,0,int(bool(rid)),n//4096,0,n//4096)
      def read(self,rid,n):
       calls.append(('read',rid));assert byid[rid][1]==n
       if rid in gids:return b'\xa5'*4096
       i=next(i for i,b in enumerate(bindings) if b[2]==rid)
       return (source/f'model-bo-{i}.bin').read_bytes() if bindings[i][3]==1 else bytes(n)
      def request(self,action,rid=0):assert action in (p.UNMAP,p.UNPIN,p.DESTROY,p.CLOSE);calls.append((action,rid))
     def child(argv,**kwargs):
      assert kwargs['pass_fds']==(99,)
      for f in source.iterdir():
       if f.is_file():shutil.copyfile(f,output/f.name)
      if fault=='receipt':
       f=output/'RESPONSE.bin';b=bytearray(f.read_bytes());struct.pack_into('<I',b,124,99);f.write_bytes(b)
      if fault=='guards':(output/'GUARD_VA.bin').write_bytes(struct.pack('<4Q',gvas[0],gvas[1]-4096,*gvas[2:]))
      if fault=='extras':(output/'EXTRAS.bin').write_bytes(struct.pack('<QQII',extras[0][0],extras[0][1],gids[0],0)+struct.pack('<QQII',*extras[1]))
      return subprocess.CompletedProcess(argv,1 if fault=='child' else 0)
     client=args.model_output/'model-client'
     with patch.object(p,'device_identity',return_value={'scope':'SYSCALL_MODEL_GPU0'}),patch.object(native.os,'open',return_value=99),patch.object(native.os,'close') as close,patch.object(native.subprocess,'run',side_effect=child),patch.object(p,'Memory',Memory):
      if fault:
       with self.assertRaises(RuntimeError):native.collect(Path('/never-device'),output,plan,images,client,args.catalogue)
       self.assertFalse(any(isinstance(action,int) for action,rid in calls))
      else:
       report=native.collect(Path('/never-device'),output,plan,images,client,args.catalogue)
       self.assertEqual(report['memory_cleanup'],'complete');self.assertEqual(report['status'],'RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING');self.assertEqual(len(report['object_info']),23);self.assertFalse(report['reference_images_uploaded_by_collector']);self.assertTrue(report['guards_intact']);self.assertEqual(len([1 for action,rid in calls if action==p.DESTROY]),23);self.assertIn((p.CLOSE,0),calls)
      close.assert_called_once_with(99)
if __name__=='__main__':unittest.main(argv=['test_scene_native.py'])
