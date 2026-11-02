#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify the bounded LAB clip/edge premises from preserved input bytes.

CPU proof only: no rendered image, device access, uploaded vertex calculation
or claim that the new CODE344 program has executed on hardware.
"""
import argparse
import hashlib
import json
import struct
from fractions import Fraction as F
from pathlib import Path

from build_gears_clamped_api_probe_profiles import WITNESS_SHA256
from gears_homogeneous_oracle import VERTICES_SHA, COEFFICIENT_SHA


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--api-inputs', type=Path, required=True)
    parser.add_argument('--witness', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pn_path = args.api_inputs / 'fixed-GL-PN-3.bin'
    source_path = args.api_inputs / 'fixed-GL-PARAM-3.bin'
    old_pn_path = args.witness / 'vertices-source.bin'
    old_c_path = args.witness / 'coefficients-source.bin'
    pn, source = pn_path.read_bytes(), source_path.read_bytes()
    old_pn, old_c = old_pn_path.read_bytes(), old_c_path.read_bytes()
    sha = lambda data: hashlib.sha256(data).hexdigest()
    assert len(pn) == 96 and len(source) == 160
    assert sha(pn) == WITNESS_SHA256['vertices']
    assert sha(source[4:148]) == WITNESS_SHA256['coefficient']
    assert sha(old_pn) == VERTICES_SHA and sha(old_c) == COEFFICIENT_SHA
    words = lambda data: struct.unpack('<' + 'I' * (len(data) // 4), data)
    assert [i for i, (a, b) in enumerate(zip(words(pn), words(old_pn))) if a != b] == [7, 15, 23]
    assert [i for i, (a, b) in enumerate(zip(words(source[4:148]), words(old_c))) if a != b] == [7, 11, 15]
    vertices = list(map(F, struct.unpack('<24f', pn)))
    coefficients = list(map(F, struct.unpack('<36f', source[4:148])))
    clip = [tuple(sum(vertices[8*v+t] * coefficients[16+4*t+k]
                      for t in range(4)) for k in range(4)) for v in range(3)]
    assert all(p[3] == 2 and p[2] == 0 and
               all(-p[3] < c < p[3] for c in p[:3]) for p in clip)
    cases = []
    for width, height in [(1280, 800), (640, 640)]:
        window = [(F(width, 2) * (p[0]/p[3] + 1),
                   F(height, 2) * (1 - p[1]/p[3])) for p in clip]
        # Convex hull is strictly inside this bounded footprint. Pixels outside
        # it cannot lie on any finite triangle edge or inside the triangle.
        assert all(0 < x < 48 and 0 < y < 48 for x, y in window)
        inside = 0
        for y in range(48):
            for x in range(48):
                sample = (F(2*x+1, 2), F(2*y+1, 2))
                edges = [(b[0]-a[0])*(sample[1]-a[1]) -
                         (b[1]-a[1])*(sample[0]-a[0])
                         for a, b in zip(window, window[1:]+window[:1])]
                assert not (any(e == 0 for e in edges) and all(e >= 0 for e in edges))
                inside += all(e > 0 for e in edges)
        assert inside == (135 if width == 1280 else 55)
        cases.append({'size': f'{width}x{height}', 'window': [[str(c) for c in p] for p in window],
                      'sample_edge_ties': 0, 'analytic_interior_samples': inside})
    result = {'status': 'FINITE_RECIPE_PREMISES_PASS', 'GPU_operations': 0,
              'physical_qualification': False, 'clip': [[str(c) for c in p] for p in clip],
              'strictly_inside_six_clip_planes': True, 'cases': cases,
              'scope': 'Only sealed W2 positions/matrix, full viewport, no culling, filled triangles, half-pixel grid. No generic clipping/edge claim or new shader execution proof.',
              'inputs_sha256': {str(p): sha(p.read_bytes()) for p in
                               [pn_path, source_path, old_pn_path, old_c_path]}}
    with args.output.open('x') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print('PASS strict clip bounds and no finite sample-edge ties at both sizes; CPU proof/GPU0')


if __name__ == '__main__':
    main()
