#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline captured frontend/session bytes only; never physical provenance or FPS.
Requires independent presealed oracles and trace. No device access or execution.
"""
import argparse
import json
from pathlib import Path
import struct
import gears_scene_native as native
import gears_scene_profile as scene
import gears_scene_trace as trace
from verify_general_session_capture import need, sha, request, response

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
    plan, images = scene.load_profile(catalogue, size, asset_root, homogeneous=True)
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
    objects = native.inventory(before, context, 25)
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
        need((job/'ADAPTER-DISPATCH.bin').read_bytes() == struct.pack('<i', 1)
             and (job/'DISPATCH.bin').read_bytes() == struct.pack('<i', 1), 'provider dispatch outcome')
        need(struct.unpack('<2Q', (job/'HOST-OP.bin').read_bytes()) == (context, sequence)
             and struct.unpack('<3Q', (job/'HOST-RECEIPT.bin').read_bytes()) == (context, sequence, 7),
             'distinct authenticated frontend/provider operations')
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
        need((job/'RETAINED-COLOR-BGRA.bin').read_bytes() == color == expected_color, 'held COLOR frontend read')
        final_color = color
        outcomes.append(dict(sequence=sequence, receipt=receipt, control_words_exact=66,
                             READ_inputs_unchanged=13, guards_intact=4, COLOR_mismatches=color_bad, Z24_mismatches=z_bad))
    close = request(3, context, session=session, sequence=2)
    need((capture/'CLOSE-REQUEST.bin').read_bytes() == close and (capture/'CLOSE-RESULT.bin').read_bytes() == bytes(4), 'CLOSE request/result')
    response((capture/'CLOSE-RESPONSE.bin').read_bytes(), close, 3)
    need((capture/'RESULT.bin').read_bytes() == bytes(4), 'client outcome')
    native.inventory((capture/'INVENTORY-REMAINING.bin').read_bytes(), context, 7)
    return dict(status='CAPTURED_FRONTEND_GENERAL_ARCHIVE_MATCHES_EXISTING_ORACLES', physical_qualification=False,
                GPU_operations_proven_by_this_tool=0, API_draws_in_report=0, frontend_submissions=2, session_receipts=2,
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
