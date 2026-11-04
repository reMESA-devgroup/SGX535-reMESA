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


def verify(capture, fixture, oracle, size, manifest, manifest_sha, *,
           session_ready=False, frame_serial=1, final_ownership=None):
    # Default one-shot checks stay exact. Continuation explicitly proves job
    # retirement/READY rather than pretending source/backing are CLOSED.
    need((session_ready and final_ownership is not None and 1 <= frame_serial <= 100)
         or (not session_ready and frame_serial == 1 and final_ownership is None), 'archive lifetime mode')
    expected, bindings, expectation = preflight(fixture, oracle, size, manifest, manifest_sha)
    return _verify_frame(capture, fixture, oracle, size, expected, bindings, expectation,
                         session_ready=session_ready, frame_serial=frame_serial, final_ownership=final_ownership)


def _verify_frame(capture, fixture, oracle, size, expected, bindings, expectation, *,
                  session_ready=False, frame_serial=1, final_ownership=None,
                  preserve_depth=False, expected_color=None, expected_depth=None, packet_matcher=None):
    # Shared archive mechanics only. Callers must preflight their own sealed
    # inputs/oracles. No source authority, device identity or GPU proof here.
    need(not preserve_depth or (session_ready and final_ownership is not None), 'preserve lifetime mode')
    need((expected_color is None) == (expected_depth is None), 'complete pixel expectation pair')
    if session_ready:
        expected = bytearray(expected)
        struct.pack_into('<Q', expected, 8, frame_serial)
        expected = bytes(expected)  # planned serial, never copied from observed packet
    final = final_ownership if session_ready else capture
    raw = (capture/'MATRIX-RESPONSE.bin').read_bytes()
    need(len(raw) == 2344, 'matrix envelope size')
    version, budget, flags, reserved, context = struct.unpack_from('<4IQ', raw)
    need((version, budget, flags, reserved) == (1, 1000, 5 if preserve_depth else 1 if session_ready else 0, 0) and context, 'matrix request prefix')
    packet = (capture/'packet.bin').read_bytes()
    need(raw[24:2288] == packet, 'submitted packet identity')
    ids = [struct.unpack_from('<I', packet, 32+8*i)[0] for i in range(18)]
    (packet_matches if packet_matcher is None else packet_matcher)(packet, expected, context, ids)
    op, cookie, core, rev, pds, phase, ledger, error, retained, stages, r0, r1 = struct.unpack_from('<QQ5Ii4I', raw, 2288)
    need(op and op <= 0xffffffff and cookie == op and
         (core, rev, pds, phase, ledger, error, retained, stages, r0, r1) ==
         (0x01130000, 0x10201, 0x20000000, 9, 7, 0, 0, 23 if session_ready else 15, 0, 0), 'native retirement/closure')
    need((capture/'MATRIX-DISPATCH.bin').read_bytes() == struct.pack('<I', 1) and
         (capture/'MATRIX-RESPONSE-RESULT.bin').read_bytes() == bytes(4), 'matrix adapter outcome')
    host = (capture/'MATRIX-RECEIPT.bin').read_bytes()
    ticket = (capture/'MATRIX-HOST-OP.bin').read_bytes()
    need(len(host) == 20 and struct.unpack_from('<Q', host)[0] == context and
         len(ticket) == 16 and host[:16] == ticket and
         struct.unpack_from('<Q', host, 8)[0] and host[16:] == b'\1\1\1\0', 'i486 host retirement receipt')
    width, height = map(int, size.split('x'))
    color_id, depth_id = struct.unpack_from('<II', packet, 352)
    fop = None
    if preserve_depth:
        need(not list(capture.glob('FILL-*')), 'no-clear draw unexpectedly cleared')
    else:
        fill_request = (capture/'FILL-REQUEST.bin').read_bytes()
        fill = (capture/'FILL-RESPONSE.bin').read_bytes()
        need(len(fill) == len(fill_request) == 104 and fill_request[:16] == bytes(16)
             and not any(fill_request[64:]), 'fill request/size')
        need(fill[:8] == struct.pack('<II', 1, 0) and struct.unpack_from('<Q', fill, 8)[0] == context
             and fill[16:64] == fill_request[16:64], 'native fill echo')
        fop, fcook, fstages, fret, fcore, frev, ferr, fres = struct.unpack_from('<QQ4IiI', fill, 64)
        need(struct.unpack_from('<12I', fill, 16) ==
             (color_id, width, height, width*4, 0, 0, width, height, 0, 1000, 0, 0), 'actual full COLOR clear')
        need(0 < fop < op and fcook and (fstages, fret, fcore, frev, ferr, fres) ==
             (31, 0, 0x01130000, 0x10201, 0, 0) and
             (capture/'FILL-RESULT.bin').read_bytes() == bytes(4), 'fill completion/retirement')
    guards = struct.unpack('<4I', (capture/'GUARD_IDS.bin').read_bytes())
    owner_snapshot = inventory((capture/'INVENTORY-OWNERS.bin').read_bytes(), context,
                               24 if session_ready and frame_serial > 1 else 20)
    owners = [entry for entry in owner_snapshot if entry[2] not in guards]
    need(len(owners) == 20, 'nonguard source owner inventory')
    before = (capture/'INVENTORY-BEFORE.bin').read_bytes()
    objects = inventory(before, context, 24)
    need(before == (capture/'INVENTORY-AFTER.bin').read_bytes(), 'allocation inventory changed')
    byid = {e[2]:e for e in objects}
    need(all(byid.get(ident) == (va, n, ident, 1) for va, n, ident, _ in owner_snapshot), 'source owner identity')
    where = (capture/'placement.bin').read_bytes()
    need(len(where) == 776 and where[:8] == struct.pack('<II', 1, 18) and not any(where[440:]), 'actual placement')
    for i, (va, n, _, access) in enumerate(bindings):
        need(byid.get(ids[i]) == (va, n, ids[i], 1) and
             struct.unpack_from('<QQII', where, 8+24*i) == (va, n, ids[i], access), 'bound memory identity')
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
    color_reference = (oracle/'COLOR.bin').read_bytes() if expected_color is None else expected_color
    expected_z = (oracle/'Z24.bin').read_bytes() if expected_depth is None else expected_depth
    need(len(color_reference) == len(expected_z) == width*height*4, 'pixel expectation extent')
    need(color == color_reference, 'COLOR mismatch')
    need(len(depth) == len(expected_z) and all((a[0]^b[0]) & 0xffffff == 0 for a, b in
         zip(struct.iter_unpack('<I', depth), struct.iter_unpack('<I', expected_z))), 'Z24 mismatch')
    gl = (capture/'GL-READPIXELS-BGRA.bin').read_bytes()
    stride = width*4
    need(gl == b''.join(color[y*stride:(y+1)*stride] for y in range(height-1, -1, -1)) and
         sha(gl) == expectation['GL_readback_bottom_up_sha256'], 'actual GL readback/origin')
    need((capture/'RESULT.bin').read_bytes() == bytes(4) and
         struct.unpack('<12IQ', (capture/'OPENGL-REPORT.bin').read_bytes()) ==
         (1, 1, 13, 0, 1, 0, 0, 1, 0, 1, len(color), 0, 0), 'actual ordinary OpenGL report')
    remaining = inventory((final/'INVENTORY-REMAINING.bin').read_bytes(), context, 6)
    need({e[2] for e in remaining} == set(guards) | {color_id, depth_id} and
         all(byid.get(e[2]) == e for e in remaining), 'remaining lifetime ownership')
    info = struct.unpack('<3Q10I', (final/'INFO-RETURN.bin').read_bytes())
    need(info == (sum(e[1] for e in remaining), 0, 0, 6, 6,
                  sum(e[1]//4096 for e in remaining), 0, 0, 0, 0, 0, 0, 0),
         'returned endpoint not idle or inconsistent global inventory')
    t = struct.unpack('<5Q', (capture/'TIMES.bin').read_bytes())
    ft = None if preserve_depth else struct.unpack('<2Q', (capture/'FILL-TIMES.bin').read_bytes())
    need(all(b >= a for a, b in zip(t, t[1:])) and
         (ft is None or t[0] <= ft[0] <= ft[1] <= t[1]), 'phase timing order')
    result = dict(status='ORDINARY_MATRIX_ARCHIVE_MATCHES_PRESEALED_EXPECTATIONS',
                physical_qualification=False, GPU_operations_proven_by_this_tool=0,
                context=context, operation=op, fill_operation=fop,
                CORE=core, revision=rev, phase=phase, ledger=ledger, stages=stages,
                COLOR_mismatches=0, Z24_mismatches=0, guards_intact=4,
                remaining=remaining, timings_ns=dict(zip(
                    ('frontend_prepare_including_fill', 'diagnostic_pre_reads', 'native_submit_wall', 'diagnostic_retired_reads'),
                    [b-a for a, b in zip(t, t[1:])])), fill_wall_ns=None if ft is None else ft[1]-ft[0],
                scope='Archive checks only; outer physical provenance, source identity and stock recovery remain mandatory')
    if session_ready:
        result.update(status='ORDINARY_CONTINUOUS_FRAME_MATCHES_PRESEALED_EXPECTATIONS',
                      frame_serial=frame_serial, host_operation=struct.unpack_from('<Q', ticket, 8)[0],
                      attachment_ids=[color_id, depth_id], session_ready=True, session_closed=False)
    return result


def verify_continuous(capture, fixture, oracle, size, manifest, manifest_sha, draws):
    need(draws in (2, 10, 100), 'bounded continuous draw count')
    directories = sorted(p.name for p in capture.glob('frame-*'))
    need(directories == [f'frame-{i:06d}' for i in range(1, draws+1)], 'exact frame inventory')
    frames = []
    previous_native = previous_host = 0
    for i in range(1, draws+1):
        checked = verify(capture/f'frame-{i:06d}', fixture, oracle, size, manifest, manifest_sha,
                         session_ready=True, frame_serial=i, final_ownership=capture)
        need(checked['fill_operation'] > previous_native and checked['operation'] > checked['fill_operation']
             and checked['host_operation'] > previous_host, 'continuous operation order')
        if frames:
            need(checked['context'] == frames[0]['context'] and
                 checked['attachment_ids'] == frames[0]['attachment_ids'] and
                 checked['remaining'] == frames[0]['remaining'], 'same session/attachments/guards')
        previous_native, previous_host = checked['operation'], checked['host_operation']
        frames.append(checked)
    close = (capture/'CLOSE-RESPONSE.bin').read_bytes()
    request = (capture/'CLOSE-REQUEST.bin').read_bytes()
    prefix = struct.pack('<4IQ', 1, 0, 2, 0, frames[0]['context']) + bytes(2264)
    need(len(close) == len(request) == 2344 and request == prefix+bytes(56), 'pure close request')
    need(close[:2288] == prefix and struct.unpack_from('<QQ5Ii4I', close, 2288) ==
         (0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0), 'idle source/backing close receipt')
    need((capture/'CLOSE-RESULT.bin').read_bytes() == bytes(4) and
         (capture/'CLOSE-RESPONSE-RESULT.bin').read_bytes() == bytes(4), 'close transport result')
    return dict(status='ORDINARY_CONTINUOUS_ARCHIVE_MATCHES_PRESEALED_EXPECTATIONS',
                physical_qualification=False, GPU_operations_proven_by_this_tool=0,
                context=frames[0]['context'], draws=draws, native_2D_fills=draws,
                COLOR_mismatches=0, Z24_mismatches=0, guards_per_frame=4,
                source_backing_closed=True, remaining=frames[-1]['remaining'], frames=frames,
                scope='Archive only; independent boot/device/kernel identity, source and stock recovery audit still required')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'fixture', 'oracle', 'manifest'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--manifest-sha256', required=True)
    p.add_argument('--size', choices=['1280x800', '640x640'], required=True)
    a = p.parse_args()
    print(json.dumps(verify(a.capture, a.fixture, a.oracle, a.size, a.manifest, a.manifest_sha256), indent=2))
