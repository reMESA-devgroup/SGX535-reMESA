#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pixel-gate tests against retained physical evidence; no GPU operation."""
import argparse
import json
from pathlib import Path
import struct
import tempfile
import unittest

import gears_scene_oracle as oracle

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--catalogue',type=Path,required=True)
parser.add_argument('--old-witness',type=Path,required=True)
parser.add_argument('--model-output',type=Path,required=True)
args = parser.parse_args()


class Pixels(unittest.TestCase):
    def test_old_physical_witness_and_whole_target_clear(self):
        c = json.loads(args.catalogue.read_text())
        v = Path(c['witness']['vertices_path']).read_bytes()
        coeff = Path(c['witness']['coefficient_path']).read_bytes()
        color,depth,_,_ = oracle.expected(v,coeff,1280,800)
        # Two retained files, including actual authenticated physical pixels.
        self.assertEqual(color,(args.old_witness/'expected-color-corrected.bin').read_bytes())
        self.assertEqual(color,(args.old_witness/'hardware/actual-control/capture/color-snapshot.bin').read_bytes())
        old = (args.old_witness/'hardware/actual-control/capture/z-snapshot.bin').read_bytes()
        changed = 0
        for i,((a,),(b,)) in enumerate(zip(struct.iter_unpack('<I',depth),struct.iter_unpack('<I',old))):
            if i%1280<48 and i//1280<48:
                self.assertEqual(a&0xffffff,b&0xffffff)
            else:
                self.assertEqual(a&0xffffff,0xffffff)
                self.assertEqual(b,0)
                changed += 1
        self.assertEqual(changed,1280*800-48*48)

    def test_sealed_pixels_and_failures(self):
        for size in ['1280x800','640x640']:
            with self.subTest(size=size),tempfile.TemporaryDirectory() as d:
                root = Path(d);expected = root/'expected';capture = root/'capture';capture.mkdir()
                m = oracle.prepare(args.catalogue,size,expected)
                digest = oracle.sha((expected/'ORACLE.json').read_bytes())
                cp = capture/f"bo-{m['color_index']}-readback.bin"
                zp = capture/f"bo-{m['depth_index']}-readback.bin"
                color = (expected/'COLOR.bin').read_bytes();depth = (expected/'Z24.bin').read_bytes()
                cp.write_bytes(color);zp.write_bytes(depth)
                self.assertTrue(oracle.verify(expected,capture,digest)['pixel_match'])
                # Z24X8 ignores only X8; depth and every COLOR bit remain gated.
                x8 = bytearray(depth);x8[3::4] = b'\xa5'*(len(x8)//4);zp.write_bytes(x8)
                self.assertTrue(oracle.verify(expected,capture,digest)['pixel_match'])
                x8[0] ^= 1;zp.write_bytes(x8)
                self.assertEqual(oracle.verify(expected,capture,digest)['Z24_mismatched_pixels'],1)
                zp.write_bytes(depth);bad = bytearray(color);bad[3] ^= 1;cp.write_bytes(bad)
                self.assertEqual(oracle.verify(expected,capture,digest)['color_mismatched_pixels'],1)
                cp.write_bytes(color[:-4])
                with self.assertRaises(ValueError):oracle.verify(expected,capture,digest)
                cp.write_bytes(color)
                with self.assertRaises(ValueError):oracle.verify(expected,capture,'0'*64)
                # Actual syscall model never rasterizes: a valid modeled
                # lifecycle must fail the independent physical pixel gate.
                source = args.model_output/(size+'--execute')
                placement = (source/'placement.bin').read_bytes()
                for index,path in [(m['color_index'],cp),(m['depth_index'],zp)]:
                    ident = struct.unpack_from('<I',placement,8+24*index+16)[0]
                    path.write_bytes((source/f'after-id-{ident}.bin').read_bytes())
                result = oracle.verify(expected,capture,digest)
                self.assertFalse(result['pixel_match'])
                self.assertEqual(result['color_mismatched_pixels'],118)
                self.assertEqual(result['Z24_mismatched_pixels'],len(depth)//4)
                self.assertFalse(result['physical_qualification'])
                (expected/'ORACLE.json').write_text('{}')
                with self.assertRaises(ValueError):oracle.verify(expected,capture,digest)

    def test_other_inputs_are_not_approved(self):
        c = json.loads(args.catalogue.read_text())
        v = Path(c['witness']['vertices_path']).read_bytes()
        coeff = Path(c['witness']['coefficient_path']).read_bytes()
        for altered in [bytes(len(v)),v[:-1],v+b'\0']:
            with self.assertRaises(ValueError):oracle.expected(altered,coeff,1280,800)
        with self.assertRaises(ValueError):oracle.expected(v,bytes(len(coeff)),1280,800)
        with self.assertRaises(ValueError):oracle.expected(v,coeff,641,479)


if __name__ == '__main__':
    unittest.main(argv=['test_gears_scene_oracle.py'])
