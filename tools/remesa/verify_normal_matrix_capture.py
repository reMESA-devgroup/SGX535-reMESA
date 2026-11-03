#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Ordinary API qualification archive checks; no device/boot/GPU proof by itself."""
import argparse
import json
from pathlib import Path
import struct
from gears_scene_native import inventory, packet_matches
from normal_matrix_oracle import sha, inputs


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def preflight(fixture, oracle, size, manifest, manifest_sha):
    raw = manifest.read_bytes()
    need(sha(raw) == manifest_sha, 'qualification prelaunch manifest changed')
    seal = json.loads(raw)
    need(seal['format'] == 'REMESA_ORDINARY_MATRIX_QUALIFICATION_V1' and
         seal['size'] == size and seal['GPU_operations'] == 0 and
         seal['physical_qualification'] is False, 'qualification scope')
    required = {'packet.bin', 'placement.bin', 'vertices-source.bin', 'coefficients-source.bin'}
    packet = (fixture/'packet.bin').read_bytes()
    need(len(packet) == 2264 and struct.unpack_from('<4I', packet, 16) == (1, 18, 11, 2), 'typed packet topology')
    ids = [struct.unpack_from('<I', packet, 32+8*i)[0] for i in range(18)]
    need(len(set(ids)) == 18 and all(ids), 'fixture binding identity')
    bindings = []
    placement = (fixture/'placement.bin').read_bytes()
    need(len(placement) == 776 and struct.unpack_from('<II', placement) == (1, 18), 'placement header')
    need(not any(placement[440:]), 'placement unused capacity')
    for i in range(18):
        va, length, ident, access = struct.unpack_from('<QQII', placement, 8+i*24)
        need((ident, access) == struct.unpack_from('<II', packet, 32+i*8) and
             access in (1, 3) and va and va % 4096 == 0 and length and
             length % 4096 == 0 and va+length <= 2**32, 'fixture binding bounds')
        need(all(not (va < a+n and a < va+length) for a, n, _, _ in bindings), 'fixture overlap')
        bindings.append((va, length, ident, access))
        if access == 1:
            required.add(f'bo-{i}.bin')
            need(len((fixture/f'bo-{i}.bin').read_bytes()) == length, 'READ image extent')
    need(set(seal['fixture']) == required, 'fixture inventory')
    for name in required:
        p = fixture/name
        need(not p.is_symlink() and sha(p.read_bytes()) == seal['fixture'][name], 'fixture prelaunch pin')
    need(((fixture/'vertices-source.bin').read_bytes(),
          (fixture/'coefficients-source.bin').read_bytes()) == inputs(), 'ordinary input witness')
    need(set(seal['oracle']) == {'ORACLE.json', 'COLOR.bin', 'Z24.bin'}, 'oracle inventory')
    for name, digest in seal['oracle'].items():
        need(not (oracle/name).is_symlink() and sha((oracle/name).read_bytes()) == digest, 'oracle prelaunch pin')
    expectation = json.loads((oracle/'ORACLE.json').read_text())
    need(expectation['classification'] == 'CPU_EXPECTATION_ONLY_NOT_GPU' and expectation['size'] == size
         and expectation['physical_qualification'] is False and expectation['GPU_operations'] == 0
         and expectation['sample_on_edge'] is False, 'oracle scope')
    width, height = map(int, size.split('x'))
    need(struct.unpack_from('<8I', packet, 320) == (2, 80, width, height, width, width, 4, 0x3f800000), 'attachment contract')
    color, depth = struct.unpack_from('<II', packet, 352)
    need(color in ids and depth in ids and color != depth, 'attachment identity')
    for ident in (color, depth):
        need(bindings[ids.index(ident)][1] == width*height*4 and bindings[ids.index(ident)][3] == 3, 'attachment allocation')
    for name in ('COLOR.bin', 'Z24.bin'):
        data = (oracle/name).read_bytes()
        need(len(data) == width*height*4 and sha(data) == expectation['files'][name], 'oracle pixels')
    need(expectation['vertices_sha256'] == seal['fixture']['vertices-source.bin'] and
         expectation['coefficients_sha256'] == seal['fixture']['coefficients-source.bin'], 'oracle input identity')
    return packet, bindings, expectation


def verify(capture, fixture, oracle, size, manifest, manifest_sha):
    expected, bindings, expectation = preflight(fixture, oracle, size, manifest, manifest_sha)
    raw = (capture/'MATRIX-RESPONSE.bin').read_bytes()
    need(len(raw) == 2344, 'matrix envelope size')
    version, budget, flags, reserved, context = struct.unpack_from('<4IQ', raw)
    need((version, budget, flags, reserved) == (1, 1000, 0, 0) and context, 'matrix request prefix')
    packet = (capture/'packet.bin').read_bytes()
    need(raw[24:2288] == packet, 'submitted packet identity')
    ids = [struct.unpack_from('<I', packet, 32+8*i)[0] for i in range(18)]
    packet_matches(packet, expected, context, ids)
    op, cookie, core, rev, pds, phase, ledger, error, retained, stages, r0, r1 = struct.unpack_from('<QQ5Ii4I', raw, 2288)
    need(op and op <= 0xffffffff and cookie == op and
         (core, rev, pds, phase, ledger, error, retained, stages, r0, r1) ==
         (0x01130000, 0x10201, 0x20000000, 9, 7, 0, 0, 15, 0, 0), 'native retirement/closure')
    need((capture/'MATRIX-DISPATCH.bin').read_bytes() == struct.pack('<I', 1) and
         (capture/'MATRIX-RESPONSE-RESULT.bin').read_bytes() == bytes(4), 'matrix adapter outcome')
    host = (capture/'MATRIX-RECEIPT.bin').read_bytes()
    ticket = (capture/'MATRIX-HOST-OP.bin').read_bytes()
    need(len(host) == 20 and struct.unpack_from('<Q', host)[0] == context and
         len(ticket) == 16 and host[:16] == ticket and
         struct.unpack_from('<Q', host, 8)[0] and host[16:] == b'\1\1\1\0', 'i486 host retirement receipt')
    fill_request = (capture/'FILL-REQUEST.bin').read_bytes()
    fill = (capture/'FILL-RESPONSE.bin').read_bytes()
    need(len(fill) == len(fill_request) == 104 and fill_request[:16] == bytes(16)
         and not any(fill_request[64:]), 'fill request/size')
    need(fill[:8] == struct.pack('<II', 1, 0) and struct.unpack_from('<Q', fill, 8)[0] == context
         and fill[16:64] == fill_request[16:64], 'native fill echo')
    fop, fcook, fstages, fret, fcore, frev, ferr, fres = struct.unpack_from('<QQ4IiI', fill, 64)
    width, height = map(int, size.split('x'))
    color_id, depth_id = struct.unpack_from('<II', packet, 352)
    need(struct.unpack_from('<12I', fill, 16) ==
         (color_id, width, height, width*4, 0, 0, width, height, 0, 1000, 0, 0), 'actual full COLOR clear')
    need(0 < fop < op and fcook and (fstages, fret, fcore, frev, ferr, fres) ==
         (31, 0, 0x01130000, 0x10201, 0, 0) and
         (capture/'FILL-RESULT.bin').read_bytes() == bytes(4), 'fill completion/retirement')
    owners = inventory((capture/'INVENTORY-OWNERS.bin').read_bytes(), context, 20)
    before = (capture/'INVENTORY-BEFORE.bin').read_bytes()
    objects = inventory(before, context, 24)
    need(before == (capture/'INVENTORY-AFTER.bin').read_bytes(), 'allocation inventory changed')
    byid = {e[2]:e for e in objects}
    need(all(byid.get(ident) == (va, n, ident, 1) for va, n, ident, _ in owners), 'source owner identity')
    where = (capture/'placement.bin').read_bytes()
    need(len(where) == 776 and where[:8] == struct.pack('<II', 1, 18) and not any(where[440:]), 'actual placement')
    for i, (va, n, _, access) in enumerate(bindings):
        need(byid.get(ids[i]) == (va, n, ids[i], 1) and
             struct.unpack_from('<QQII', where, 8+24*i) == (va, n, ids[i], access), 'bound memory identity')
    guards = struct.unpack('<4I', (capture/'GUARD_IDS.bin').read_bytes())
    addresses = struct.unpack('<4Q', (capture/'GUARD_VA.bin').read_bytes())
    expected_guards = []
    for lo, hi in ((0x20000000, 0x28000000), (0x30000000, 0x40000000), (0x40000000, 0xe0000000)):
        group = [e for e in owners if lo <= e[0] < hi]
        if group:
            expected_guards.extend(a for a in (min(e[0] for e in group)-4096, max(e[0]+e[1] for e in group)) if lo <= a and a+4096 <= hi)
    need(list(addresses) == expected_guards and len(set(guards)) == 4 and
         set(byid) == {e[2] for e in owners} | set(guards), 'enclosing guard ownership')
    for ident, address in zip(guards, addresses):
        need(byid.get(ident) == (address, 4096, ident, 1), 'guard mapping')
        for when in ('before', 'after'):
            need((capture/f'{when}-id-{ident}.bin').read_bytes() == b'\xa5'*4096, 'guard changed')
    for _, n, ident, _ in objects:
        if ident in ids and bindings[ids.index(ident)][3] != 1:
            continue
        a = (capture/f'before-id-{ident}.bin').read_bytes()
        b = (capture/f'after-id-{ident}.bin').read_bytes()
        need(len(a) == n and a == b, 'READ/CPU owner changed')
        if ident in ids:
            need(a == (fixture/f'bo-{ids.index(ident)}.bin').read_bytes(), 'READ source identity')
    color = (capture/f'after-id-{color_id}.bin').read_bytes()
    depth = (capture/f'after-id-{depth_id}.bin').read_bytes()
    need(color == (oracle/'COLOR.bin').read_bytes(), 'COLOR mismatch')
    expected_z = (oracle/'Z24.bin').read_bytes()
    need(len(depth) == len(expected_z) and all((a[0]^b[0]) & 0xffffff == 0 for a, b in
         zip(struct.iter_unpack('<I', depth), struct.iter_unpack('<I', expected_z))), 'Z24 mismatch')
    gl = (capture/'GL-READPIXELS-BGRA.bin').read_bytes()
    stride = width*4
    need(gl == b''.join(color[y*stride:(y+1)*stride] for y in range(height-1, -1, -1)) and
         sha(gl) == expectation['GL_readback_bottom_up_sha256'], 'actual GL readback/origin')
    need((capture/'RESULT.bin').read_bytes() == bytes(4) and
         struct.unpack('<12IQ', (capture/'OPENGL-REPORT.bin').read_bytes()) ==
         (1, 1, 13, 0, 1, 0, 0, 1, 0, 1, len(color), 0, 0), 'actual ordinary OpenGL report')
    remaining = inventory((capture/'INVENTORY-REMAINING.bin').read_bytes(), context, 6)
    need({e[2] for e in remaining} == set(guards) | {color_id, depth_id} and
         all(byid.get(e[2]) == e for e in remaining), 'remaining lifetime ownership')
    info = struct.unpack('<3Q10I', (capture/'INFO-RETURN.bin').read_bytes())
    need(info == (sum(e[1] for e in remaining), 0, 0, 6, 6,
                  sum(e[1]//4096 for e in remaining), 0, 0, 0, 0, 0, 0, 0),
         'returned endpoint not idle or inconsistent global inventory')
    t = struct.unpack('<5Q', (capture/'TIMES.bin').read_bytes())
    ft = struct.unpack('<2Q', (capture/'FILL-TIMES.bin').read_bytes())
    need(all(b >= a for a, b in zip(t, t[1:])) and t[0] <= ft[0] <= ft[1] <= t[1], 'phase timing order')
    return dict(status='ORDINARY_MATRIX_ARCHIVE_MATCHES_PRESEALED_EXPECTATIONS',
                physical_qualification=False, GPU_operations_proven_by_this_tool=0,
                context=context, operation=op, fill_operation=fop,
                CORE=core, revision=rev, phase=phase, ledger=ledger, stages=stages,
                COLOR_mismatches=0, Z24_mismatches=0, guards_intact=4,
                remaining=remaining, timings_ns=dict(zip(
                    ('frontend_prepare_including_fill', 'diagnostic_pre_reads', 'native_submit_wall', 'diagnostic_retired_reads'),
                    [b-a for a, b in zip(t, t[1:])])), fill_wall_ns=ft[1]-ft[0],
                scope='Archive checks only; outer physical provenance, source identity and stock recovery remain mandatory')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'fixture', 'oracle', 'manifest'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--manifest-sha256', required=True)
    p.add_argument('--size', choices=['1280x800', '640x640'], required=True)
    a = p.parse_args()
    print(json.dumps(verify(a.capture, a.fixture, a.oracle, a.size, a.manifest, a.manifest_sha256), indent=2))
