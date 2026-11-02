#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check bounded API/session capture bytes against presealed existing oracles.

No device access. A passing archive does not establish physical provenance:
boot/module/source/control/recovery evidence must be authenticated separately.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import gears_scene_native as native
import gears_scene_profile as scene
import gears_scene_trace as trace


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def request(action, context, session=0, sequence=0, ids=None, profile=0, maximum=0):
    return (struct.pack('<IIQQQ', 1, action, context, session, sequence)
            + struct.pack('<18I4I', *(ids or [0]*18), profile,
                          1000 if action == 2 else 0, maximum, 0)
            + bytes(56))


def response(raw, before, action):
    need(len(raw) == len(before) == 176, 'session envelope size')
    if action == 1:
        need(raw[:16] == before[:16] and raw[24:120] == before[24:120], 'OPEN prefix changed')
    else:
        need(raw[:120] == before[:120], 'session request prefix changed')
    operation, cookie = struct.unpack_from('<QQ', raw, 120)
    core, revision, pds, phase, ledger, error, state, stages, r0, r1 = struct.unpack_from('<5Ii4I', raw, 136)
    need(error == r0 == r1 == 0, 'session error/reserved response')
    if action == 3:
        need((state, stages) == (3, 32) and not any((operation, cookie, core, revision, pds, phase, ledger)), 'CLOSE receipt')
    else:
        need((core, revision, pds, state) == (0x01130000, 0x10201, 0x20000000, 1), 'SGX identity/state')
        if action == 1:
            need(not any((operation, cookie, phase, ledger)) and stages == 16, 'OPEN is not a GPU receipt')
        else:
            need(operation > 0 and cookie == operation and (phase, ledger, stages) == (9, 7, 15), 'retirement receipt')
    return dict(operation=operation, cookie=cookie, core=core, revision=revision,
                pds_base=pds, phase=phase, ledger=ledger, error=error, retained=0, stages=stages)


def verify(capture, catalogue, size, oracle, oracle_sha, controls, controls_sha,
           plan_trace, plan_trace_sha, asset_root=None):
    manifest_raw = (oracle/'ORACLE.json').read_bytes()
    need(sha(manifest_raw) == oracle_sha, 'oracle prelaunch pin')
    manifest = json.loads(manifest_raw)
    need(manifest['classification'] == 'CPU_EXPECTATION_ONLY_NOT_GPU' and manifest['size'] == size, 'oracle scope')
    expected_color, expected_z = [(oracle/n).read_bytes() for n in ('COLOR.bin', 'Z24.bin')]
    need(sha(expected_color) == manifest['files']['COLOR.bin'] and sha(expected_z) == manifest['files']['Z24.bin'], 'oracle data pin')
    control_raw = controls.read_bytes()
    need(len(control_raw) == 264 and sha(control_raw) == controls_sha, '66 control word prelaunch pin')
    baseline = plan_trace.read_bytes()
    need(len(baseline) == 4296 and sha(baseline) == plan_trace_sha, 'qualified plan trace prelaunch pin')
    baseline = baseline[16:]
    plan, images = scene.load_profile(catalogue, size, asset_root, clamped_api=True)
    opened = (capture/'OPEN-REQUEST.bin').read_bytes()
    need(len(opened) == 176, 'OPEN size')
    context = struct.unpack_from('<Q', opened, 8)[0]
    ids = list(struct.unpack_from('<18I', opened, 32))
    need(context and len(set(ids)) == 18 and all(ids), 'OPEN context/IDs')
    need(opened == request(1, context, ids=ids, profile=plan['profile'], maximum=2), 'closed two-job OPEN')
    need((capture/'OPEN-RESULT.bin').read_bytes() == bytes(4), 'OPEN call result')
    raw = (capture/'OPEN-RESPONSE.bin').read_bytes()
    response(raw, opened, 1)
    session = struct.unpack_from('<Q', raw, 16)[0]
    need(session > 0, 'server session handle')
    fixture = (asset_root/size if asset_root else Path(next(p for p in json.loads(catalogue.read_text())['profiles'] if p['size'] == size)['packet_path']).parent)
    expected_packet = (fixture/'packet.bin').read_bytes()
    native.packet_matches((capture/'packet.bin').read_bytes(), expected_packet, context, ids)
    before = (capture/'INVENTORY-BEFORE.bin').read_bytes()
    objects = native.inventory(before, context, 24)
    need(len(images) == 13, 'closed READ image set')
    guard_ids = list(struct.unpack('<4I', (capture/'GUARD_IDS.bin').read_bytes()))
    need(len(set(guard_ids)) == 4 and not set(guard_ids) & set(ids), 'guard identity')
    for index, data in images.items():
        need((capture/f'before-id-{ids[index]}.bin').read_bytes() == data, 'pre-submit READ image')
    outcomes, previous, final_color = [], 0, None
    for sequence in (1, 2):
        job = capture/f'session-{sequence:06d}'
        req = request(2, context, session=session, sequence=sequence)
        need((job/'REQUEST.bin').read_bytes() == req and (job/'RESULT.bin').read_bytes() == bytes(4), 'SUBMIT request/result')
        receipt = response((job/'RESPONSE.bin').read_bytes(), req, 2)
        need(receipt['operation'] > previous, 'nonmonotonic operation')
        previous = receipt['operation']
        qreq, qraw = (job/'TRACE-REQUEST.bin').read_bytes(), (job/'TRACE-RESPONSE.bin').read_bytes()
        need(qreq == trace.QUERY.pack(2, 0, context, bytes(4280)) and len(qraw) == 4296
             and qraw[:16] == qreq[:16] and (job/'TRACE-RESULT.bin').read_bytes() == bytes(4), 'trace query/result')
        decoded = trace.decode(qraw[16:], context, receipt, plan)
        need(qraw[16+128:16+1800] == baseline[128:1800]
             and qraw[16+56:16+60] == baseline[56:60], 'qualified full action/USE plan differs')
        need([b['id'] for b in decoded['bindings']] == ids, 'trace binding IDs')
        need(struct.pack('<66I', *(decoded['raster'] + decoded['ta'])) == control_raw, '66 control words differ')
        samples = [[e['sequence'], e['status1'], e['status2']] for e in decoded['events'] if e['kind'] == 4]
        op = receipt['operation']
        need(samples in ([[op, 8192, 0], [op, 262144, 0], [op, 1, 0]],
                         [[op, 8192, 0], [op, 262145, 0]]), 'attributable completion status sequence')
        need((job/'INVENTORY-AFTER.bin').read_bytes() == before, 'job changed allocation inventory')
        for _, n, ident, _ in objects:
            if ident in ids and plan['bindings'][ids.index(ident)]['access'] != 1:
                continue
            a, b = (capture/f'before-id-{ident}.bin').read_bytes(), (job/f'after-id-{ident}.bin').read_bytes()
            need(len(a) == len(b) == n and a == b, 'READ/CPU-owner/guard changed')
        for ident in guard_ids:
            need((job/f'after-id-{ident}.bin').read_bytes() == b'\xa5'*4096, 'guard content changed')
        color = (job/f"after-id-{ids[plan['color_index']]}.bin").read_bytes()
        z = (job/f"after-id-{ids[plan['depth_index']]}.bin").read_bytes()
        need(len(color) == len(expected_color) and len(z) == len(expected_z), 'output extents')
        color_bad = sum(a != b for a, b in zip(struct.iter_unpack('<I', color), struct.iter_unpack('<I', expected_color)))
        z_bad = sum((a[0] ^ b[0]) & 0xffffff != 0 for a, b in zip(struct.iter_unpack('<I', z), struct.iter_unpack('<I', expected_z)))
        need(color_bad == z_bad == 0, f'COLOR/Z24 oracle mismatch in job{sequence}: {color_bad}/{z_bad}')
        final_color = color
        outcomes.append(dict(sequence=sequence, receipt=receipt, control_words_exact=66,
                             READ_inputs_unchanged=13, guards_intact=4, COLOR_mismatches=color_bad, Z24_mismatches=z_bad))
    close = request(3, context, session=session, sequence=2)
    need((capture/'CLOSE-REQUEST.bin').read_bytes() == close and (capture/'CLOSE-RESULT.bin').read_bytes() == bytes(4), 'CLOSE request/result')
    response((capture/'CLOSE-RESPONSE.bin').read_bytes(), close, 3)
    width, height = map(int, size.split('x'))
    need((capture/'RESULT.bin').read_bytes() == bytes(4), 'client outcome')
    need(struct.unpack('<12IQ', (capture/'OPENGL-REPORT.bin').read_bytes()) ==
         (1, 1, 13, 0, 1, 0, 0, 1, 0, 1, width*height*4, 0, 0), 'actual API report')
    gl = (capture/'GL-READPIXELS-BGRA.bin').read_bytes()
    stride = width*4
    need(gl == b''.join(final_color[y*stride:(y+1)*stride] for y in range(height-1, -1, -1)), 'glReadPixels origin/content')
    need(sha(gl) == manifest['GL_readback_bottom_up_sha256'], 'presealed GL readback')
    native.inventory((capture/'INVENTORY-REMAINING.bin').read_bytes(), context, 6)
    return dict(status='SESSION_CAPTURE_BYTES_MATCH_EXISTING_ORACLES', physical_qualification=False,
                GPU_operations_proven_by_this_tool=0, API_draws_in_report=1, session_receipts=2,
                jobs=outcomes, oracle_manifest_sha256=oracle_sha, control_words_sha256=controls_sha,
                scope='Archive/content checks only. Physical boot/module/source/control/recovery provenance and remaining supervisor cleanup must be verified separately. No FPS claim.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'catalogue', 'oracle', 'controls', 'plan-trace'):
        p.add_argument('--'+name, required=True, type=Path)
    p.add_argument('--size', required=True, choices=['1280x800', '640x640'])
    p.add_argument('--oracle-sha256', required=True)
    p.add_argument('--controls-sha256', required=True)
    p.add_argument('--plan-trace-sha256', required=True)
    p.add_argument('--asset-root', type=Path)
    a = p.parse_args()
    print(json.dumps(verify(a.capture, a.catalogue, a.size, a.oracle,
                            a.oracle_sha256, a.controls, a.controls_sha256,
                            a.plan_trace, a.plan_trace_sha256, a.asset_root), indent=2))


if __name__ == '__main__':
    main()
