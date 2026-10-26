#!/usr/bin/env python3
"""Mini12 full1280x800 depth diagnostic expectation. Never opens a GPU.

The original32x32 reference remains unchanged. Geometry is explicitly scaled
window-space, not a CPU-rendered replacement for application geometry. A match
must have independent native submission/retirement/ownership authentication.
"""
import argparse
import hashlib
import json
from pathlib import Path
import depth_occlusion_oracle as small

WIDTH, HEIGHT = 1280, 800
TRIANGLE = ((160, 100), (1120, 100), (160, 675))
ROW_BYTES = WIDTH * 4
FRAME_BYTES = ROW_BYTES * HEIGHT


def interval(y):
    if not 100 <= y < 675:
        return 0, 0
    # Strict sloped half-plane at half-integer sample centers. Twice the
    # expression is odd (575*odd +960*odd), so there are no edge ties.
    remaining = 2*960*575 - 960*(2*y+1-200)
    last = min(1119, (remaining+575*(320-1)-1)//(2*575))
    return (160, last+1) if last >= 160 else (0, 0)


def inside_edges(x, y):
    # Independent geometric determinant formulation, used for row-boundary
    # controls. No division or floating point tolerance.
    p = (2*x+1, 2*y+1)
    vertices = tuple((2*a, 2*b) for a, b in TRIANGLE)
    values = []
    for i, a in enumerate(vertices):
        b = vertices[(i+1) % 3]
        values.append((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))
    assert all(v != 0 for v in values)
    return all(v > 0 for v in values)


def rows(compare='LESS', write=True, reverse=False):
    model = small.expected(compare, write, reverse)
    color = model[(5*32+5)*4:(5*32+5)*4+4]
    for y in range(HEIGHT):
        first, end = interval(y)
        yield small.BLACK*first + color*(end-first) + small.BLACK*(WIDTH-end)


def validate_model():
    count = 0
    for y in range(HEIGHT):
        first, end = interval(y)
        count += end-first
        probes = {0, 159, 160, 1119, 1279, first-1, first, end-1, end}
        for x in probes:
            if 0 <= x < WIDTH:
                assert inside_edges(x, y) == (first <= x < end)
    assert count == 276000
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--expected', type=Path)
    parser.add_argument('--compare', choices=('LESS', 'ALWAYS', 'NEVER'), default='LESS')
    parser.add_argument('--no-write', action='store_true')
    parser.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    count = validate_model()
    expected_hash, actual_hash = hashlib.sha256(), hashlib.sha256()
    capture = args.capture.open('rb') if args.capture else None
    output = args.expected.open('xb') if args.expected else None
    match = True
    try:
        for row in rows(args.compare, not args.no_write, args.reverse):
            assert len(row) == ROW_BYTES
            expected_hash.update(row)
            if output:
                output.write(row)
            if capture:
                observed = capture.read(ROW_BYTES)
                actual_hash.update(observed)
                match = match and observed == row
        if capture:
            match = match and not capture.read(1)
    finally:
        if capture:
            capture.close()
        if output:
            output.close()
    result = {'status': 'CPU_ORACLE_ONLY', 'width': WIDTH, 'height': HEIGHT,
              'stride': ROW_BYTES, 'frame_bytes': FRAME_BYTES,
              'triangle': TRIANGLE, 'covered_pixels': count,
              'outside_pixels': WIDTH*HEIGHT-count,
              'expected_sha256': expected_hash.hexdigest(), 'GPU_submissions': 0,
              'physical_depth': 'NOT_ESTABLISHED'}
    if args.capture:
        result.update(color_model_matches=match, actual_sha256=actual_hash.hexdigest(),
                      provenance='MUST_BE_AUTHENTICATED_SEPARATELY')
    print(json.dumps(result, indent=2))
    return 0 if match else 1


if __name__ == '__main__':
    raise SystemExit(main())
