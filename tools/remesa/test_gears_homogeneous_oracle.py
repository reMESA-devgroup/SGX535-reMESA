#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only checks of the sealed W2 pixel expectation; no GPU simulation."""
import argparse,json,tempfile,unittest
from pathlib import Path
import gears_homogeneous_oracle as oracle
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture-root',type=Path,required=True);a=p.parse_args()
class Oracle(unittest.TestCase):
 def test_both_sizes_and_refusals(self):
  for i,size in enumerate(['1280x800','640x640']):
   with self.subTest(size=size),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);expected=root/'expected';capture=root/'capture';capture.mkdir();fixture=a.capture_root/('export-'+str(i))/size
    m=oracle.prepare(fixture,size,expected);digest=oracle.sha((expected/'ORACLE.json').read_bytes());self.assertEqual(m['white_pixels'],135 if i==0 else 55);self.assertFalse(m['physical_qualification'])
    color=(expected/'COLOR.bin').read_bytes();depth=(expected/'Z24.bin').read_bytes();cp=capture/f"bo-{m['color_index']}-readback.bin";zp=capture/f"bo-{m['depth_index']}-readback.bin";cp.write_bytes(color);zp.write_bytes(depth)
    self.assertTrue(oracle.verify(expected,capture,digest)['pixel_match'])
    cp.write_bytes(bytes(len(color)));zp.write_bytes(bytes(len(depth)));r=oracle.verify(expected,capture,digest);self.assertFalse(r['pixel_match']);self.assertEqual(r['color_mismatched_pixels'],m['white_pixels']);self.assertEqual(r['Z24_mismatched_pixels'],len(depth)//4)
    cp.write_bytes(color);bad=bytearray(depth);bad[3::4]=b'\x5a'*(len(depth)//4);zp.write_bytes(bad);self.assertTrue(oracle.verify(expected,capture,digest)['pixel_match']);bad[0]^=1;zp.write_bytes(bad);self.assertEqual(oracle.verify(expected,capture,digest)['Z24_mismatched_pixels'],1)
    zp.write_bytes(depth);bad=bytearray(color);bad[3]^=1;cp.write_bytes(bad);self.assertEqual(oracle.verify(expected,capture,digest)['color_mismatched_pixels'],1)
    with self.assertRaises(ValueError):oracle.verify(expected,capture,'0'*64)
    cp.write_bytes(color[:-4])
    with self.assertRaises(ValueError):oracle.verify(expected,capture,digest)
 def test_unsupported_target(self):
  with tempfile.TemporaryDirectory() as tmp:
   with self.assertRaises(ValueError):oracle.prepare(a.capture_root/'export-0/1280x800','641x479',Path(tmp)/'oracle')
if __name__=='__main__':unittest.main(argv=['test_gears_homogeneous_oracle.py'])
