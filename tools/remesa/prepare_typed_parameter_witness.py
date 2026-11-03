#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Preseal two closed LAB expectations; no device access or rendering.

Reuses the physically checked W=2 witness and its existing oracle. One exact
dyadic translation moves it 10 or5 pixels right, without changing CODE, W, Z,
lighting, topology or sample-edge rules. These bytes are never GPU inputs or
presented as rendering; actual identity/retirement/readbacks remain mandatory.
"""
import argparse
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path
import shutil
import struct
from gears_homogeneous_oracle import prepare

def sha(blob):
    return hashlib.sha256(blob).hexdigest()

def build(fixture, size, output):
    width, height = map(int, size.split('x'))
    output.mkdir(parents=True, exist_ok=False)
    original = {p.name: sha(p.read_bytes()) for p in fixture.iterdir() if p.is_file()}
    first = prepare(fixture, size, output/'first')
    old = (fixture/'coefficients-source.bin').read_bytes()
    values = list(struct.unpack('<36f', old))
    if values[28] != -2:
        raise ValueError('not the sealed original translation')
    values[28] += 1/32
    updated = struct.pack('<36f', *values)
    if any(old[i:i+4] != updated[i:i+4] for i in range(0, 144, 4) if i != 112):
        raise ValueError('changed more than the one matrix coefficient')
    # Independent exact rational check of the projected translation. This
    # computes expectations only; application inputs still go to the GPU.
    raw = struct.unpack('<24f', (fixture/'vertices-source.bin').read_bytes())
    columns = [list(map(F, values[16+4*k:20+4*k])) for k in range(4)]
    clip = [tuple(sum(F(raw[8*i+t])*columns[t][k] for t in range(4)) for k in range(4)) for i in range(3)]
    if any(p[2] != 0 or p[3] != 2 for p in clip):
        raise ValueError('changed depth or homogeneous divide')
    window = [(F(width, 2)*(p[0]/p[3]+1), F(height, 2)*(1-p[1]/p[3])) for p in clip]
    dx = 10 if width == 1280 else 5
    before = [tuple(F(v) for v in point) for point in first['window']]
    if window != [(p[0]+dx, p[1]) for p in before]:
        raise ValueError('not an exact integral pixel translation')
    second = dict(first)
    second.update(coefficients_sha256=sha(updated), clip=[[str(v) for v in p] for p in clip],
                  window=[[str(v) for v in p] for p in window],
                  translation_pixels=[dx, 0],
                  predecessor_manifest_sha256=sha((output/'first/ORACLE.json').read_bytes()))
    second['files'] = {}
    (output/'second').mkdir()
    for name, background in [('COLOR.bin', b'\0'*4), ('Z24.bin', b'\xff\xff\xff\0')]:
        source = (output/'first'/name).read_bytes()
        rows = []
        for y in range(height):
            row = source[4*width*y:4*width*(y+1)]
            if row[-4*dx:] != background*dx:
                raise ValueError('translation would discard non-background pixels')
            rows.append(background*dx+row[:-4*dx])
        changed = b''.join(rows)
        if changed == source or len(changed) != len(source):
            raise ValueError('expectations must differ at equal extent')
        (output/'second'/name).write_bytes(changed)
        second['files'][name] = sha(changed)
    (output/'second/ORACLE.json').write_text(json.dumps(second, indent=2)+'\n')
    shutil.copytree(fixture, output/'fixture')
    (output/'fixture/coefficients-update.bin').write_bytes(updated)
    if any(sha((fixture/name).read_bytes()) != digest for name, digest in original.items()):
        raise ValueError('original witness changed during preparation')
    result = {'classification': 'CPU_EXPECTATIONS_ONLY_NOT_GPU', 'GPU_operations': 0,
              'physical_qualification': False, 'size': size,
              'matrix_coefficient': 28, 'PARAM_byte_offset': 116,
              'translation_pixels': [dx, 0], 'original_files': original,
              'files': {str(p.relative_to(output)): sha(p.read_bytes())
                        for p in sorted(output.rglob('*')) if p.is_file()}}
    (output/'WITNESS.json').write_text(json.dumps(result, indent=2)+'\n')
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--size', choices=['1280x800', '640x640'], required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = build(a.fixture, a.size, a.output)
    print(json.dumps({k: result[k] for k in ['classification', 'size', 'translation_pixels', 'GPU_operations']}))

if __name__ == '__main__':
    main()
