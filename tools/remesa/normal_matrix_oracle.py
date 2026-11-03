#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent expectation for one bounded ordinary GL input; never GPU input.

Reuse the physically established rational sample-centre construction. Reject
ties instead of guessing an unqualified hardware edge rule. No device access.
"""
import argparse
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path
import struct


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inputs():
    c = [0.]*36
    for i in (0, 1, 2, 3, 16, 21, 26, 34):
        c[i] = 1.
    c[28], c[29], c[31] = .25, 1./256, 2.
    v = [-.5, -.5, 0, 1, 0, 0, 1, 0,
         .5, -.5, 0, 1, 0, 0, 1, 0,
         -.5, .5, 0, 1, 0, 0, 1, 0]
    return struct.pack('<24f', *v), struct.pack('<36f', *c)


def expected(vertices, coefficients, size):
    if (vertices, coefficients) != inputs():
        raise ValueError('not the explicitly bounded ordinary matrix witness')
    if size not in ('1280x800', '640x640'):
        raise ValueError('unsupported qualification dimensions')
    width, height = map(int, size.split('x'))
    v = list(map(F, struct.unpack('<24f', vertices)))
    c = list(map(F, struct.unpack('<36f', coefficients)))
    clip = [tuple(sum(v[i*8+t]*c[16+t*4+k] for t in range(4))
                  for k in range(4)) for i in range(3)]
    if any(p[2] != 0 or p[3] != 2 for p in clip):
        raise ValueError('unexpected homogeneous/depth contract')
    window = [(width*F(1, 2)*(p[0]/p[3]+1),
               height*F(1, 2)*(1-p[1]/p[3])) for p in clip]
    if any(not (0 < x < width and 0 < y < height) for x, y in window):
        raise ValueError('unexpected clipping')
    scale = 256
    points = [(int(x*scale), int(y*scale)) for x, y in window]
    if any((F(x, scale), F(y, scale)) != p for (x, y), p in zip(points, window)):
        raise ValueError('non-exact integer edge scale')
    cross = lambda a, b, p: (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
    direction = 1 if cross(window[0], window[1], window[2]) > 0 else -1
    edges = list(zip(points, points[1:]+points[:1]))
    color = bytearray(width*height*4)
    depth = bytearray(b'\xff\xff\xff\0'*(width*height))
    covered = 0
    for y in range(int(min(p[1] for p in window)), int(max(p[1] for p in window))+1):
        for x in range(int(min(p[0] for p in window)), int(max(p[0] for p in window))+1):
            e = [direction*cross(a, b, (x*scale+scale//2, y*scale+scale//2)) for a, b in edges]
            if all(value >= 0 for value in e) and any(value == 0 for value in e):
                raise ValueError('sample on edge; no unqualified tie rule')
            if all(value > 0 for value in e):
                covered += 1
                struct.pack_into('<I', color, 4*(y*width+x), 0xffffffff)
                struct.pack_into('<I', depth, 4*(y*width+x), 0x800000)
    return bytes(color), bytes(depth), window, covered


def prepare(fixture, size, output):
    vertices = (fixture/'vertices-source.bin').read_bytes()
    coefficients = (fixture/'coefficients-source.bin').read_bytes()
    color, depth, window, covered = expected(vertices, coefficients, size)
    width, height = map(int, size.split('x'))
    stride = width*4
    gl = b''.join(color[y*stride:(y+1)*stride] for y in range(height-1, -1, -1))
    output.mkdir(parents=True, exist_ok=False)
    (output/'COLOR.bin').write_bytes(color)
    (output/'Z24.bin').write_bytes(depth)
    result = dict(classification='CPU_EXPECTATION_ONLY_NOT_GPU', size=size,
                  GPU_operations=0, physical_qualification=False,
                  vertices_sha256=sha(vertices), coefficients_sha256=sha(coefficients),
                  window=[[str(x), str(y)] for x, y in window], white_pixels=covered,
                  sample_on_edge=False, files={'COLOR.bin':sha(color), 'Z24.bin':sha(depth)},
                  GL_readback_bottom_up_sha256=sha(gl),
                  scope='One pinned ordinary input, existing sample-centre/Z24 rules; never uploaded or presented')
    (output/'ORACLE.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--size', choices=['1280x800', '640x640'], required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    print(json.dumps(prepare(a.fixture, a.size, a.output), indent=2))
