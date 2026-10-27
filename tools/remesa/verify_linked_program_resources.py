#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Audit this bounded resource probe's memory transcript, not GPU execution."""
import argparse
import collections
import json
from pathlib import Path
import struct

REQUEST = struct.Struct('<4I4Q2I')
HEADER = struct.Struct('<6I')
SIZE = 4152

def verify(blob):
    if not __debug__:
        raise RuntimeError('Run this assertion-based auditor without Python optimization')
    assert len(blob) % (2 * (HEADER.size + SIZE)) == 0
    objects = {}; counts = collections.Counter(); programs = []; surfaces = []
    context = None; opened = False; closed = False; cursor = 0
    for _ in range(len(blob) // (2 * (HEADER.size + SIZE))):
        pair = []
        for phase in (0, 1):
            header = HEADER.unpack_from(blob, cursor); cursor += HEADER.size
            assert header == (0x50434c52, phase, 0, 0, SIZE, 0)
            pair.append(blob[cursor:cursor+SIZE]); cursor += SIZE
        before, after = pair
        a, b = REQUEST.unpack_from(before), REQUEST.unpack_from(after)
        version, action, ident, flags, ctx, size, va, offset, length, reserved = a
        assert version == 1 and 1 <= action <= 14 and not reserved and not closed
        assert b[:2] == a[:2] and b[3] == flags and b[5:] == a[5:]
        if action != 1: assert b[2] == ident
        if action != 12: assert b[4] == ctx == context
        if action not in (8, 14): assert before[56:] == after[56:]
        if action != 9: assert not any(before[56:])
        assert flags == 0 or (action == 7 and flags == 1)
        assert not size or action == 1
        assert not va or action == 3
        assert not (offset or length) or action in (8, 9)
        counts[action] += 1
        if action == 12:
            assert not opened and not ctx and b[4] and not objects
            context = b[4]; opened = True; continue
        assert opened
        if action == 13:
            assert not objects and not ident
            closed = True; continue
        if action == 14:
            expected = [sum(x['size'] for x in objects.values()), 0, 0,
                        len(objects), sum(x['pinned'] for x in objects.values()),
                        sum(x['size']//4096 for x in objects.values() if x['va']),
                        sum(x['lease'] is not None for x in objects.values()), 0, 0, 0, 0, 0, 0]
            if ident:
                x = objects[ident]
                expected[1:3] = [x['size'], x['va']]
                expected[9:] = [int(x['pinned']), x['size']//4096 if x['va'] else 0,
                                int(x['lease'] is not None), x['size']//4096 if x['va'] else 0]
            assert list(struct.unpack_from('<3Q10I', after, 56)) == expected
            assert not any(after[120:]); continue
        if action == 1:
            new = b[2]; assert not ident and new not in objects and new>>5
            assert size and size % 4096 == 0
            objects[new] = {'size': size, 'va': 0, 'pinned': False,
                            'lease': None, 'shadow': bytearray(size) if size == 4096 else None,
                            'reads': 0, 'writes': 0}
            continue
        x = objects[ident]
        if action == 2:
            assert not x['pinned']; x['pinned'] = True
        elif action == 3:
            assert x['pinned'] and not x['va'] and va and va % 4096 == 0
            assert va+x['size'] <= 0x100000000
            assert all(not y['va'] or va+x['size'] <= y['va'] or y['va']+y['size'] <= va for y in objects.values())
            x['va'] = va
            if va == 0x20080000:
                assert x['size'] == 4096; programs.append(x)
            else:
                assert va >= 0x40001000; surfaces.append(x['size'])
        elif action == 4:
            assert x['va'] and x['lease'] is None; x['va'] = 0
        elif action == 5:
            assert x['pinned'] and not x['va'] and x['lease'] is None; x['pinned'] = False
        elif action == 6:
            assert not x['pinned'] and not x['va'] and x['lease'] is None; del objects[ident]
        elif action == 7:
            assert x['pinned'] and x['lease'] is None; x['lease'] = flags
        elif action in (8, 9):
            assert x in programs and x['lease'] is not None and offset == 0 and length == 4096
            if action == 9:
                assert x['lease'] == 1
                x['shadow'][:] = before[56:]; x['writes'] += 1
                assert any(x['shadow'][:64]) and any(x['shadow'][160:168])
                assert not any(x['shadow'][64:160]) and not any(x['shadow'][168:])
            else:
                assert after[56:] == x['shadow']; x['reads'] += 1
        elif action == 10:
            assert x['lease'] is not None; x['lease'] = None
        elif action == 11:
            assert x['pinned'] and x['lease'] is None
    assert closed and not objects and cursor == len(blob)
    expected_counts = {1:8,2:8,3:8,4:8,5:8,6:8,7:6,8:6,9:2,10:6,11:2,12:1,13:1,14:12}
    assert dict(counts) == expected_counts
    assert surfaces == [4096000,4096000,1638400,1638400,1290240,1290240]
    assert len(programs) == 2 and all(x['writes']==1 and x['reads']==3 for x in programs)
    assert programs[0]['shadow'][:64] == programs[1]['shadow'][:64]
    assert programs[0]['shadow'][160:168] != programs[1]['shadow'][160:168]
    return {'status':'LINKED_CODE_RESOURCE_TRANSCRIPT_PASS_NOT_RENDERING', 'memory_calls':sum(counts.values()),
            'GPU_submissions':0,'API_contexts':0,'final_INFO_zero_and_CLOSE':True,
            'program_full_readback_matches_upload':True,'surface_allocation_bytes':surfaces,
            'scope':'CPU/GEM upload, PTE ownership counts and cleanup; not SGX instruction fetch, retirement or OpenGL/GLES rendering.'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--transcript',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); data=args.transcript.read_bytes(); result=verify(data)
    args.output.write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps(result))

if __name__=='__main__': main()
