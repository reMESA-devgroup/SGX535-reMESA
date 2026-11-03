#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the explicit two-job typed-matrix LAB archive, never physical provenance.
Separate from the unchanged v1 verifier. No device access, execution or FPS.
"""
import argparse
import json
from pathlib import Path
import struct
import gears_scene_native as native
import gears_scene_profile as scene
import gears_scene_trace as trace
from verify_general_session_capture import need, sha, request as v1_request, response
from gears_homogeneous_oracle import VERTICES_SHA, COEFFICIENT_SHA


def request(*args, **kwargs):
    wire = v1_request(*args, **kwargs)
    return struct.pack('<I', 2) + wire[4:]


def witness_inputs(witness, witness_sha, size):
    raw = (witness/'WITNESS.json').read_bytes()
    need(sha(raw) == witness_sha, 'witness prelaunch pin')
    meta = json.loads(raw)
    need(meta['classification'] == 'CPU_EXPECTATIONS_ONLY_NOT_GPU'
         and meta['physical_qualification'] is False and meta['GPU_operations'] == 0
         and meta['size'] == size, 'witness scope')
    dx = 10 if size == '1280x800' else 5
    need(size in ('1280x800', '640x640') and meta['matrix_coefficient'] == 28
         and meta['PARAM_byte_offset'] == 116 and meta['translation_pixels'] == [dx, 0],
         'closed matrix update recipe')
    files = meta['files']
    for name, digest in files.items():
        path = Path(name)
        need(not path.is_absolute() and '..' not in path.parts, 'witness path escape')
        need(sha((witness/path).read_bytes()) == digest, 'witness file changed')
    old = (witness/'fixture/coefficients-source.bin').read_bytes()
    changed = (witness/'fixture/coefficients-update.bin').read_bytes()
    need(sha(old) == COEFFICIENT_SHA and len(changed) == len(old) == 144
         and sha((witness/'fixture/vertices-source.bin').read_bytes()) == VERTICES_SHA,
         'sealed original inputs')
    need(old[112:116] == struct.pack('<f', -2) and changed[112:116] == struct.pack('<f', -1.96875)
         and changed[:112] == old[:112] and changed[116:] == old[116:], 'exact one-word matrix change')
    expectations = []
    for name in ('first', 'second'):
        blob = (witness/name/'ORACLE.json').read_bytes()
        need(sha(blob) == files[name+'/ORACLE.json'], 'oracle manifest pin')
        manifest = json.loads(blob)
        need(manifest['classification'] == 'CPU_EXPECTATION_ONLY_NOT_GPU' and manifest['size'] == size,
             'oracle scope')
        outputs = [(witness/name/n).read_bytes() for n in ('COLOR.bin', 'Z24.bin')]
        for n, data in zip(('COLOR.bin', 'Z24.bin'), outputs):
            need(sha(data) == manifest['files'][n] == files[name+'/'+n], 'oracle data pin')
        expectations.append(outputs)
    need(expectations[0][0] != expectations[1][0] and expectations[0][1] != expectations[1][1],
         'two different frame expectations required')
    return old, changed, expectations


def preflight(catalogue, size, witness, witness_sha, controls, controls_sha,
              plan_trace, plan_trace_sha, asset_root=None):
    old, changed, expectations = witness_inputs(witness, witness_sha, size)
    control_raw, baseline = controls.read_bytes(), plan_trace.read_bytes()
    need(len(control_raw) == 264 and sha(control_raw) == controls_sha, '66 control word prelaunch pin')
    need(len(baseline) == 4296 and sha(baseline) == plan_trace_sha, 'qualified plan trace prelaunch pin')
    plan, images = scene.load_profile(catalogue, size, asset_root, homogeneous=True)
    fixture = (asset_root/size if asset_root else Path(next(p for p in json.loads(catalogue.read_text())['profiles'] if p['size'] == size)['packet_path']).parent)
    need((fixture/'coefficients-source.bin').read_bytes() == old
         and (fixture/'coefficients-update.bin').read_bytes() == changed, 'client update input differs from seal')
    packet = (fixture/'packet.bin').read_bytes()
    original_ids = [struct.unpack_from('<I', packet, 32+8*i)[0] for i in range(18)]
    uses = [struct.unpack_from('<6I', packet, 400+24*i) for i in range(11)]
    parameter_ids = [u[0] for u in uses if u[1] == 160 and u[2] == 8 and u[4] == 0]
    need(len(parameter_ids) == 1 and parameter_ids[0] in original_ids, 'unique actual PARAM use')
    index = original_ids.index(parameter_ids[0])
    need(index in images and len(images[index]) == 4096 and images[index][4:148] == old
         and struct.unpack_from('<I', images[index])[0] == plan['bindings'][index]['va'], 'PARAM source identity')
    changed_image = images[index][:116] + changed[112:116] + images[index][120:]
    need(len(changed_image) == 4096 and changed_image != images[index], 'exact prospective PARAM image')
    return plan, images, fixture, index, changed_image, changed, expectations, control_raw, baseline[16:]


def verify_update(capture, context, session, retired_operation, changed):
    need(len(changed) == 144, 'complete36-word coefficient update')
    update = (struct.pack('<6I4Q', 1, 0, 1, 4, 36, 0, context, session, retired_operation, 1)
              + changed + bytes(512-len(changed)) + bytes(24))
    need(len(update) == 592 and (capture/'PARAMETER-REQUEST.bin').read_bytes() == update,
         'typed data request/retired epoch')
    need((capture/'PARAMETER-RESPONSE.bin').read_bytes()
         == update[:568] + struct.pack('<Qi3I', 1, 0, 1, 0, 0), 'exact data ACK')
    need((capture/'PARAMETER-RESULT.bin').read_bytes() == bytes(4), 'typed data call result')

def verify(capture, catalogue, size, witness, witness_sha, controls, controls_sha,
           plan_trace, plan_trace_sha, asset_root=None):
    plan, images, fixture, parameter_index, changed_image, changed, expectations, control_raw, baseline = preflight(
        catalogue, size, witness, witness_sha, controls, controls_sha, plan_trace, plan_trace_sha, asset_root)
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
        expected_color, expected_z = expectations[sequence-1]
        if sequence == 2:
            verify_update(capture, context, session, previous, changed)
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
            if sequence == 2 and ident == ids[parameter_index]:
                need(a == images[parameter_index], 'initial PARAM changed')
                a = changed_image
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
                             READ_inputs_unchanged=13 if sequence == 1 else 12,
                             PARAM_expected_updates=0 if sequence == 1 else 1,
                             guards_intact=4, COLOR_mismatches=color_bad, Z24_mismatches=z_bad))
    close = request(3, context, session=session, sequence=2)
    need((capture/'CLOSE-REQUEST.bin').read_bytes() == close and (capture/'CLOSE-RESULT.bin').read_bytes() == bytes(4), 'CLOSE request/result')
    response((capture/'CLOSE-RESPONSE.bin').read_bytes(), close, 3)
    need((capture/'RESULT.bin').read_bytes() == bytes(4), 'client outcome')
    native.inventory((capture/'INVENTORY-REMAINING.bin').read_bytes(), context, 7)
    return dict(status='TYPED_PARAMETER_SESSION_ARCHIVE_MATCHES_PRESEALED_EXPECTATIONS', physical_qualification=False,
                GPU_operations_proven_by_this_tool=0, API_draws_in_report=0, frontend_submissions=2, session_receipts=2,
                data_acknowledgements=1, parameter_binding_index=parameter_index,
                jobs=outcomes, witness_manifest_sha256=witness_sha, control_words_sha256=controls_sha,
                scope='Archive/content checks only. Physical boot/module/source/control/recovery provenance and remaining supervisor cleanup must be verified separately. No FPS claim.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'catalogue', 'witness', 'controls', 'plan-trace'):
        p.add_argument('--'+name, required=True, type=Path)
    p.add_argument('--size', required=True, choices=['1280x800', '640x640'])
    p.add_argument('--witness-sha256', required=True)
    p.add_argument('--controls-sha256', required=True)
    p.add_argument('--plan-trace-sha256', required=True)
    p.add_argument('--asset-root', type=Path)
    a = p.parse_args()
    print(json.dumps(verify(a.capture, a.catalogue, a.size, a.witness,
                            a.witness_sha256, a.controls, a.controls_sha256,
                            a.plan_trace, a.plan_trace_sha256, a.asset_root), indent=2))


if __name__ == '__main__':
    main()
