#!/usr/bin/env python3
"""Prospective 32x32 depth diagnostic; NEVER submits or authenticates GPU work.

Only compare captured BGRA8 planes after independent submission, packet-order,
retirement and ownership verification. Synthetic self-tests are CPU tests.
The geometry is window-space; the physical VS/viewport transport is a separate
prerequisite. No assumption about depth texel addressing or quantization.
"""
import argparse
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path

SIZE = 32
TRIANGLE = ((4, 4), (28, 4), (4, 27))
BLACK = bytes((0, 0, 0, 255))
GREEN = bytes((0, 255, 0, 255))
MAGENTA = bytes((255, 0, 255, 255))
YELLOW = bytes((0, 255, 255, 255))


def coverage():
    # Half-integer sample centers. The sloped edge evaluates to an odd
    # numerator, so no center lies on any of the three edges.
    return tuple(x >= 4 and y >= 4 and
                 23 * (2*x + 1 - 8) + 24 * (2*y + 1 - 8) < 1104
                 for y in range(SIZE) for x in range(SIZE))


def geometric_coverage():
    def edge(a, b, p):
        return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
    out = []
    for y in range(SIZE):
        for x in range(SIZE):
            p = (F(2*x+1, 2), F(2*y+1, 2))
            e = [edge(TRIANGLE[i], TRIANGLE[(i+1) % 3], p) for i in range(3)]
            assert all(v != 0 for v in e)
            out.append(all(v > 0 for v in e))
    return tuple(out)


def expected(compare, write, reverse=False):
    if compare not in ('LESS', 'ALWAYS', 'NEVER'):
        raise ValueError('unsupported comparison')
    draws = [(F(3, 4), GREEN), (F(1, 4), MAGENTA), (F(3, 4), YELLOW)]
    if reverse:
        draws.reverse()
    z, color = F(1), BLACK
    for incoming, candidate in draws:
        passed = compare == 'ALWAYS' or (compare == 'LESS' and incoming < z)
        if passed:
            color = candidate
            if write:
                z = incoming
    return b''.join(color if inside else BLACK for inside in coverage())


def verify(data, compare, write, reverse=False):
    if len(data) != SIZE * SIZE * 4:
        raise ValueError('expected exactly 4096 BGRA8 bytes, no padding')
    return data == expected(compare, write, reverse)


def self_test():
    assert coverage() == geometric_coverage()
    a = expected('LESS', True)
    b = expected('LESS', True, True)
    assert a == b
    assert a != expected('ALWAYS', True)
    assert a != expected('NEVER', True)
    assert expected('LESS', False) == expected('ALWAYS', True)
    assert expected('ALWAYS', True) != expected('ALWAYS', True, True)
    assert not verify(bytes(4096), 'LESS', True)
    bad = bytearray(a); bad[0] ^= 1
    assert not verify(bytes(bad), 'LESS', True)
    for bad in (b'', a[:-1], a + b'\0'):
        try:
            verify(bad, 'LESS', True)
        except ValueError:
            pass
        else:
            raise AssertionError('bad extent accepted')
    return {'status': 'PASS_CPU_ORACLE_ONLY', 'inside_pixels': sum(coverage()),
            'outside_pixels': 1024-sum(coverage()), 'GPU_submissions': 0,
            'physical_depth': 'NOT_TESTED',
            'less_sha256': hashlib.sha256(a).hexdigest()}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path)
    p.add_argument('--compare', choices=('LESS', 'ALWAYS', 'NEVER'), default='LESS')
    p.add_argument('--no-write', action='store_true')
    p.add_argument('--reverse', action='store_true')
    args = p.parse_args()
    result = self_test()
    if args.capture:
        matched = verify(args.capture.read_bytes(), args.compare,
                         not args.no_write, args.reverse)
        result = {'color_model_matches': matched,
                  'GPU_provenance': 'MUST_BE_VERIFIED_SEPARATELY',
                  'depth_architecture': 'NOT_ESTABLISHED_BY_COLOR_ALONE'}
        print(json.dumps(result, indent=2))
        raise SystemExit(0 if matched else 1)
    print(json.dumps(result, indent=2))
