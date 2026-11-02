#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent closed positive-witness LAB pixel expectation; never rendering.

Reuses the rational sample-centre construction of the physically validated
20261024 oracle. Only its pinned affine W=1/unit-normal/unit-light inputs are
accepted. No device access, shader emulation or CPU-rendered presentation.
"""
import argparse
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path
import struct

import gears_scene_profile as scene

VERTICES_SHA = 'dff9a40a7aa0eedb961a56cfc99e3afb8599fa34db80aeeb9b52af011fc3bf0f'
COEFFICIENT_SHA = '39a51c01bd1453c6742b98578ae47b2ffb5b9241beb86e7c6c9818d161685dd0'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def expected(vertices, coefficients, width, height):
    if sha(vertices) != VERTICES_SHA or sha(coefficients) != COEFFICIENT_SHA:
        raise ValueError('unsupported input: this is only the sealed positive witness')
    if (width, height) not in [(1280, 800), (640, 640)]:
        raise ValueError('unsupported closed target')
    raw = struct.unpack('<24f', vertices)
    c = list(map(F, struct.unpack('<36f', coefficients)))
    columns = [c[16 + 4*i:20 + 4*i] for i in range(4)]
    inputs = [tuple(map(F, raw[8*i:8*i+4])) for i in range(3)]
    output = [tuple(sum(p[t]*columns[t][k] for t in range(4))
                    for k in range(4)) for p in inputs]
    if output != [(F(199,16), F(193,16), F(1,2), F(1)),
                  (F(455,16), F(257,16), F(1,2), F(1)),
                  (F(263,16), F(449,16), F(1,2), F(1))]:
        raise ValueError('affine witness changed')
    # Exact binary rationals, with no sample on an edge: no fill-rule guess.
    points = [(int(p[0]*32), int(p[1]*32)) for p in output]
    edges = [(-(b[1]-a[1]), b[0]-a[0],
              (b[1]-a[1])*a[0]-(b[0]-a[0])*a[1])
             for a,b in zip(points, points[1:]+points[:1])]
    color = bytearray(width*height*4)
    depth = bytearray(b'\xff\xff\xff\0'*(width*height))
    covered = []
    for y in range(8, 40):
        for x in range(8, 40):
            values = [a*(32*x+16)+b*(32*y+16)+c for a,b,c in edges]
            # Independent rational cross product checks integer edge algebra.
            sample = (F(2*x+1,2), F(2*y+1,2))
            cross = [(q[0]-p[0])*(sample[1]-p[1])-
                     (q[1]-p[1])*(sample[0]-p[0])
                     for p,q in zip(output, output[1:]+output[:1])]
            if any(v == 0 for v in values) and all(v >= 0 for v in values):
                raise ValueError('unexpected edge tie')
            inside = all(v > 0 for v in values)
            if inside != all(v > 0 for v in cross):
                raise ValueError('independent coverage disagreement')
            if inside:
                covered.append(y*width+x)
                struct.pack_into('<I', color, 4*(y*width+x), 0xffffffff)
                struct.pack_into('<I', depth, 4*(y*width+x), 0x800000)
    if len(covered) != 118:
        raise ValueError('positive witness coverage changed')
    return bytes(color), bytes(depth), output, covered


def prepare(catalogue, size, output):
    plan, _ = scene.load_profile(catalogue, size)
    c = json.loads(catalogue.read_text())
    profile = next(p for p in c['profiles'] if p['size'] == size)
    fixture = Path(profile['packet_path']).parent
    packet = (fixture/'packet.bin').read_bytes()
    width,height = map(int,size.split('x'))
    if struct.unpack_from('<8I', packet, 320) != (
            2,80,width,height,width,width,4,0x3f800000):
        raise ValueError('unexpected target pitches/format/clear-depth')
    color,depth,vertices,covered = expected(
        (fixture/'vertices-source.bin').read_bytes(),
        (fixture/'coefficients-source.bin').read_bytes(),width,height)
    if any(plan['bindings'][plan[key]]['bytes'] != len(color)
           for key in ['color_index','depth_index']):
        raise ValueError('unexpected allocation padding')
    output.mkdir(parents=True,exist_ok=False)
    (output/'COLOR.bin').write_bytes(color)
    (output/'Z24.bin').write_bytes(depth)
    manifest = {'classification':'CPU_EXPECTATION_ONLY_NOT_GPU',
                'GPU_operations':0,'physical_qualification':False,
                'size':size,'packet_sha256':sha(packet),
                'vertices_sha256':VERTICES_SHA,
                'coefficients_sha256':COEFFICIENT_SHA,
                'output_vertices':[[str(v) for v in p] for p in vertices],
                'white_pixels':len(covered),'background_color':0,
                'background_Z24':0xffffff,'foreground_Z24':0x800000,
                'depth_ignored_mask':0xff000000,
                'color_index':plan['color_index'],'depth_index':plan['depth_index'],
                'files':{'COLOR.bin':sha(color),'Z24.bin':sha(depth)},
                'scope':'LAB only; no CPU expectation is uploaded or presented'}
    (output/'ORACLE.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def verify(oracle, capture, manifest_sha256):
    manifest_bytes = (oracle/'ORACLE.json').read_bytes()
    if sha(manifest_bytes) != manifest_sha256:
        raise ValueError('oracle manifest differs from pre-submission seal')
    manifest = json.loads(manifest_bytes)
    if manifest['classification'] != 'CPU_EXPECTATION_ONLY_NOT_GPU':
        raise ValueError('not a closed CPU expectation')
    files = {name:(oracle/name).read_bytes() for name in ['COLOR.bin','Z24.bin']}
    if any(sha(data) != manifest['files'][name] for name,data in files.items()):
        raise ValueError('pinned oracle changed')
    color = (capture/('bo-'+str(manifest['color_index'])+'-readback.bin')).read_bytes()
    depth = (capture/('bo-'+str(manifest['depth_index'])+'-readback.bin')).read_bytes()
    if len(color) != len(files['COLOR.bin']) or len(depth) != len(files['Z24.bin']):
        raise ValueError('readback length mismatch')
    color_bad = sum(a != b for a,b in zip(struct.iter_unpack('<I',color),
                                        struct.iter_unpack('<I',files['COLOR.bin'])))
    depth_bad = sum(((a[0]^b[0]) & 0xffffff) != 0
                    for a,b in zip(struct.iter_unpack('<I',depth),
                                   struct.iter_unpack('<I',files['Z24.bin'])))
    return {'pixel_match':not color_bad and not depth_bad,
            'color_mismatched_pixels':color_bad,'Z24_mismatched_pixels':depth_bad,
            'physical_qualification':False,
            'scope':'Pixel comparison only; identity/submission/retirement/integrity require separate evidence',
            'oracle_manifest_sha256':sha((oracle/'ORACLE.json').read_bytes())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command',required=True)
    gen = commands.add_parser('prepare')
    gen.add_argument('--catalogue',type=Path,required=True)
    gen.add_argument('--size',choices=['1280x800','640x640'],required=True)
    gen.add_argument('--output',type=Path,required=True)
    check = commands.add_parser('verify')
    check.add_argument('--oracle',type=Path,required=True)
    check.add_argument('--manifest-sha256',required=True)
    check.add_argument('--capture',type=Path,required=True)
    args = parser.parse_args()
    if args.command == 'prepare':
        result = prepare(args.catalogue,args.size,args.output)
    else:
        result = verify(args.oracle,args.capture,args.manifest_sha256)
    print(json.dumps(result,indent=2))
    if args.command == 'verify' and not result['pixel_match']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
