#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reconstruct observed bytes from the retained caller's actual memory journal.

No device access. A valid replay proves consistency, not physical GPU origin.
The protected controller must authenticate the recorder, markers and epoch.
Writes invalidate observations; neither allocations nor writes supply read data.
"""
import hashlib
import struct

PACKET_BYTES = 4152
RECORD_BYTES = 8320
MAX_RECORDS = 4096
HEADER = struct.Struct('<4I4Q2I')
INFO = struct.Struct('<3Q10I')


def require(ok, why):
    if not ok:
        raise ValueError(why)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def packet(raw):
    require(len(raw) == PACKET_BYTES, 'packet length')
    return dict(zip(('version', 'action', 'id', 'flags', 'context', 'bytes',
                     'va', 'offset', 'length', 'reserved'), HEADER.unpack_from(raw))), raw[56:]


def reconstruct(raw, markers):
    require(type(raw) is bytes and raw and len(raw) % RECORD_BYTES == 0 and
            len(raw) <= MAX_RECORDS * RECORD_BYTES, 'bounded complete journal')
    require(type(markers) is dict and markers.keys() ==
            {'scope', 'submit_begin', 'submit_end', 'capture_end', 'capture', 'roles'},
            'exact recorder marker schema')
    require(markers['scope'] in ('INTERPOSED_CPU_ONLY', 'PROTECTED_CAPTURE_UNAUTHENTICATED'),
            'unqualified origin label')
    begin, end, stop = [markers[k] for k in ('submit_begin', 'submit_end', 'capture_end')]
    require(all(type(x) is int and x >= 0 and x % 2 == 0 for x in (begin, end, stop)) and
            0 < begin == end < stop == len(raw) // RECORD_BYTES, 'sealed journal boundaries')
    c, roles = markers['capture'], markers['roles']
    require(type(c) is dict and type(roles) is list and len(roles) == 10 and
            all(type(r) is dict and r.keys() == {'id', 'va', 'bytes'} for r in roles),
            'capture and ten roles')
    context = c['owner']
    require(type(context) is int and 0 < context < 2**64 and
            type(c['generation']) is int and c['generation'] > 0, 'owner/token epoch')
    ids = [r['id'] for r in roles] + [c['program_handle'], c['geometry_handle']]
    sizes = [r['bytes'] for r in roles] + [c['program_bytes'], c['geometry_bytes']]
    vas = [r['va'] for r in roles] + [0, 0]
    require(all(type(x) is int and 32 <= x < 2**32 for x in ids) and
            len(set(ids)) == 12, 'distinct live handles')
    require(all(type(x) is int and 0 < x <= 32*1024*1024 and x % 4096 == 0 for x in sizes),
            'allocation bounds')
    require(all(type(x) is int and 0 < x < 2**32 and x % 4096 == 0 and
                x + size <= 2**32 for x, size in zip(vas[:10], sizes[:10])), 'SGX VA bounds')
    for i in range(10):
        for j in range(i):
            require(vas[i] >= vas[j] + sizes[j] or vas[j] >= vas[i] + sizes[i], 'VA overlap')
    offset = c['source_offset']
    require(type(offset) is int and offset >= 0 and offset % 8 == 0 and
            offset <= sizes[10] - 8, 'program slice bounds')
    expected = dict(zip(ids, zip(sizes, vas)))
    tracked = ids[:6] + ids[10:]
    require(sum(expected[i][0] for i in tracked) <= 1024*1024, 'bounded observation storage')
    state, leases = {}, {}
    views = {b: {i: (bytearray(expected[i][0]), bytearray(expected[i][0]))
                 for i in tracked} for b in ('before', 'after')}
    infos = {b: {} for b in views}
    origins = {b: {i: [] for i in tracked} for b in views}
    opened = False
    for index in range(0, stop, 2):
        start = raw[index*RECORD_BYTES:(index+1)*RECORD_BYTES]
        finish = raw[(index+1)*RECORD_BYTES:(index+2)*RECORD_BYTES]
        require(struct.unpack_from('<4i', start) == (1, 0, 0, 0) and
                struct.unpack_from('<4i', finish) == (0, 0, 0, 0), 'paired successful syscall')
        require(not any(start[16+PACKET_BYTES:]) and
                start[16:16+PACKET_BYTES] == finish[16:16+PACKET_BYTES], 'request pairing')
        q, qdata = packet(start[16:16+PACKET_BYTES])
        p, data = packet(finish[16+PACKET_BYTES:])
        a, rid = q['action'], p['id']
        require(q['version'] == 1 and not q['reserved'] and 1 <= a <= 14 and
                q['length'] <= 4096, 'request ABI')
        require(q['flags'] in (0, 1) if a == 7 else q['flags'] == 0, 'request flags')
        require(q['bytes'] == 0 or a == 1, 'unused allocation size')
        require(q['va'] == 0 or a == 3, 'unused mapping address')
        require((q['offset'] == q['length'] == 0) or a in (8, 9), 'unused transfer fields')
        require(a == 9 or not any(qdata), 'unexpected request data')
        changes = {'context'} if a == 12 else {'id'} if a == 1 else set()
        require(all(p[k] == v for k, v in q.items() if k not in changes), 'response header')
        require(p['context'] == context and
                q['context'] == (0 if a == 12 else context), 'foreign context')
        require(a in (8, 14) or data == qdata, 'unexpected reply data')
        boundary = 'before' if index < begin else 'after'
        if boundary == 'after':
            require(a in (7, 8, 10, 14) and (a != 7 or q['flags'] == 0),
                    'CPU mutation in protected interval')
        if a == 12:
            require(not opened and index == 0, 'session epoch')
            opened = True
            continue
        require(opened, 'missing session open')
        if a == 14 and rid == 0:
            require(not state and not any(data), 'nonempty initial session')
            continue
        if a == 1:
            require(rid in expected and rid not in state and q['id'] == 0 and
                    q['bytes'] == expected[rid][0], 'allocation identity/lifetime')
            state[rid] = dict(size=q['bytes'], va=0, pinned=False)
            continue
        require(rid in state, 'unknown/dead backing')
        s = state[rid]
        if a == 2:
            require(not s['pinned'], 'duplicate pin')
            s['pinned'] = True
        elif a == 3:
            require(s['pinned'] and not s['va'] and q['va'] == expected[rid][1] and
                    q['va'] and q['bytes'] == 0, 'map ownership/range')
            s['va'] = q['va']
        elif a == 7:
            require(s['pinned'] and rid not in leases and q['flags'] in (0, 1), 'lease state')
            leases[rid] = q['flags']
        elif a == 10:
            require(rid in leases, 'unlease without lease')
            del leases[rid]
        elif a in (8, 9):
            first, length = p['offset'], p['length']
            require(rid in leases and length > 0 and first <= s['size'] and
                    length <= s['size'] - first, 'transfer bounds/lease')
            if a == 9:
                require(leases[rid] == 1, 'write under read lease')
            if rid in tracked:
                buf, seen = views[boundary][rid]
                if a == 9:
                    seen[first:first+length] = bytes(length)
                else:
                    for j, value in enumerate(data[:length], first):
                        require(not seen[j] or buf[j] == value, 'contradictory overlapping read')
                    buf[first:first+length] = data[:length]
                    seen[first:first+length] = b'\1' * length
                    origins[boundary][rid].append(dict(record=index+1, offset=first, bytes=length))
        elif a == 14:
            require(not any(data[INFO.size:]), 'INFO padding')
            values = list(INFO.unpack_from(data))
            require(values[1] == s['size'] and values[2] == s['va'] and
                    values[9] == int(s['pinned']) and values[11] == int(rid in leases),
                    'INFO disagrees with allocation history')
            infos[boundary][rid] = dict(id=rid, info=values)
        elif a != 11:
            raise ValueError('release/reuse or unsupported operation before authentication')
        if index + 2 == begin:
            require(not leases and set(state) == set(ids), 'issue lifetime/leases')
    require(not leases and all(set(x) == set(ids) for x in infos.values()), 'complete closed inventory')
    memory = {}
    for boundary in views:
        memory[boundary] = {}
        for rid, (buf, seen) in views[boundary].items():
            require(all(seen), 'incomplete observed coverage: '+boundary+'/'+str(rid))
            memory[boundary][str(rid)] = dict(id=rid, bytes=len(buf), sha256=sha(buf),
                data=buf.hex(), records=origins[boundary][rid])
    reads = {}
    for b in views:
        value = bytes(views[b][c['program_handle']][0][offset:offset+8])
        require(value.hex() == c['source'], 'observed source differs from token')
        reads[b] = dict(id=c['program_handle'], offset=offset, length=8,
                        bytes=value.hex(), sha256=sha(value))
    info = dict(context=context, capture=c, source_readback=reads,
        before=[infos['before'][i] for i in ids[:10]],
        after=[infos['after'][i] for i in ids[:10]],
        extra={b: [infos[b][i] for i in ids[10:]] for b in views})
    return dict(classification='JOURNAL_RECONSTRUCTION_ONLY', scope=markers['scope'],
        journal_sha256=sha(raw), journal_records=stop, context=context,
        generation=c['generation'], info=info, memory=memory)
