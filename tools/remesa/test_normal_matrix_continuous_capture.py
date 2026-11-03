#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline continuous archive checks. Synthetic test pixels never prove GPU work."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
from unittest import mock
import verify_normal_matrix_capture as archive


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--qualification', type=Path, required=True)
    p.add_argument('--models', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    results = []
    read = Path.read_bytes

    def refused(fn):
        try:
            fn()
        except (ValueError, FileNotFoundError):
            return
        raise AssertionError('invalid archive accepted')

    for size, draws in [('640x640', 2), ('1280x800', 2), ('640x640', 10), ('640x640', 100)]:
        q = a.qualification
        args = (q/'producer-v2'/size, q/('oracle-'+size), size,
                q/('MANIFEST-'+size+'.json'), archive.sha((q/('MANIFEST-'+size+'.json')).read_bytes()), draws)
        original = a.models/(size+'-'+str(draws))
        refused(lambda: archive.verify_continuous(original, *args))
        capture = a.output/('SYNTHETIC-NOT-GPU-'+size+'-'+str(draws))
        shutil.copytree(original, capture, copy_function=os.link)
        color = (args[1]/'COLOR.bin').read_bytes()
        depth = (args[1]/'Z24.bin').read_bytes()
        w, h = map(int, size.split('x'))
        gl = b''.join(color[y*w*4:(y+1)*w*4] for y in range(h-1, -1, -1))
        for i in range(1, draws+1):
            frame = capture/f'frame-{i:06d}'
            cid, zid = struct.unpack_from('<II', (frame/'packet.bin').read_bytes(), 352)
            for name, data in [(f'after-id-{cid}.bin', color), (f'after-id-{zid}.bin', depth), ('GL-READPIXELS-BGRA.bin', gl)]:
                dest = frame/name
                dest.unlink()  # Break hardlink: original model evidence stays untouched.
                dest.write_bytes(data)
        (capture/'SYNTHETIC-CLASSIFICATION.json').write_text('{"GPU_operations":0,"physical_qualification":false,"pixels":"CPU expectation for verifier tests only"}\n')
        checked = archive.verify_continuous(capture, *args)
        faults = 0
        # Mutate protocol/lifetime proofs without re-reading unchanged fixture pins.
        cached = archive.preflight(*args[:5])
        with mock.patch.object(archive, 'preflight', return_value=cached):
            for target, offsets in [
                (capture/'CLOSE-REQUEST.bin', [0, 8, 16, 24, 2288]),
                (capture/'CLOSE-RESPONSE.bin', [0, 8, 16, 24, 2288, 2332]),
                (capture/'CLOSE-RESULT.bin', [0]),
                (capture/'frame-000002'/'packet.bin', [8]),
                (capture/'frame-000002'/'MATRIX-RESPONSE.bin', [8, 32, 2288, 2332]),
                (capture/'frame-000002'/'GUARD_IDS.bin', [0]),
                (capture/'INFO-RETURN.bin', [0, 24]),
            ]:
                # Exercise identical envelope fault checks at both sizes with
                # two frames; 10/100 additionally check the complete sequence.
                if draws != 2:
                    continue
                original_bytes = read(target)
                for offset in offsets:
                    changed = bytearray(original_bytes)
                    changed[offset] ^= 1
                    with mock.patch.object(Path, 'read_bytes', lambda path, t=target, b=bytes(changed): b if path == t else read(path)):
                        refused(lambda: archive.verify_continuous(capture, *args))
                    faults += 1
        results.append(dict(size=size, draws=draws, zero_model_pixels_refused=True,
                            synthetic_archive_pass=True, rejected_faults=faults,
                            stages=[f['stages'] for f in checked['frames']]))
    result = dict(status='CONTINUOUS_ARCHIVE_OFFLINE_PASS', GPU_operations=0,
                  physical_qualification=False, runs=results,
                  sources={str(Path(f).resolve()):archive.sha(Path(f).read_bytes()) for f in (__file__, archive.__file__)})
    (a.output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status=result['status'], GPU_operations=0, runs=len(results))))


if __name__ == '__main__':
    main()
