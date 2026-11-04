#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Three ordinary draws: B baseline, clear+A, B without clear. Archive only.

Sealed COLOR expectations reuse the existing raster oracle. Farther Z24 has
an independent physical baseline, rather than assuming a new quantizer. This
tool never uploads reference pixels, touches a device or proves provenance.
"""
import json
from pathlib import Path
import struct
import verify_normal_matrix_capture as common
from normal_matrix_oracle import inputs, sha


def _packet_matches(raw, expected, context, ids):
    # Normal schema3 only. The closed LAB packet matcher stays unchanged.
    common.need(len(raw) == len(expected) == 2264 and
                struct.unpack_from('<4I',expected,16) == (1,18,10,2) and
                struct.unpack_from('<I',expected,320)[0] == 3, 'normal no-clear topology')
    old = [struct.unpack_from('<I',expected,32+8*i)[0] for i in range(18)]
    common.need(len(set(old)) == len(set(ids)) == 18 and all(old) and all(ids), 'normal packet identities')
    mapping = dict(zip(old,ids))
    normalized = bytearray(expected)
    struct.pack_into('<Q',normalized,0,context)
    offsets = [32+8*i for i in range(18)] + [304,308,312,352,356,360,372,384]
    offsets += [400+24*i for i in range(10)] + [1720,1732,1752,1764]
    for offset in offsets:
        ident = struct.unpack_from('<I',expected,offset)[0]
        common.need(ident in mapping, 'normal packet reference identity')
        struct.pack_into('<I',normalized,offset,mapping[ident])
    common.need(normalized == raw, 'normal packet differs from sealed producer')


def preflight(root, size, manifest_sha, affine_w1=False):
    manifest = root/'MANIFEST.json'
    common.need(not manifest.is_symlink() and sha(manifest.read_bytes()) == manifest_sha, 'preserve manifest pin')
    seal = json.loads(manifest.read_text())
    common.need(seal['format'] == 'REMESA_NORMAL_PRESERVE_THREE_STEP_V1' and seal['size'] == size and
                seal['GPU_operations'] == 0 and seal['physical_qualification'] is False and
                seal['native_calls'] == {'draws':3, 'fills':2} and
                seal['reference_framebuffers_uploaded'] is False, 'preserve manifest scope')
    common.need(seal.get('affine_w1', False) is affine_w1, 'affine qualification mode')
    common.need(size in ('640x640', '1280x800'), 'preserve dimensions')
    actual = {str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and p != manifest}
    common.need(actual == set(seal['files']), 'sealed input inventory')
    for name, digest in seal['files'].items():
        relative = Path(name)
        common.need(not relative.is_absolute() and '..' not in relative.parts, 'sealed path')
        p = root/relative
        common.need(not p.is_symlink() and sha(p.read_bytes()) == digest, 'sealed input changed')
    width, height = map(int, size.split('x'))
    vertices, coefficients_a = inputs()
    if affine_w1:
        values = list(struct.unpack('<36f', coefficients_a))
        values[16:32] = [v*.5 for v in values[16:32]]
        coefficients_a = struct.pack('<36f', *values)
    b = bytearray(coefficients_a)
    for i in (28, 30):
        struct.pack_into('<I', b, i*4, 0x3e800000 if affine_w1 else 0x3f000000)
    prepared = []
    for step in (1, 2, 3):
        fixture = root/f'step-{step}'
        packet = (fixture/'packet.bin').read_bytes()
        common.need(len(packet) == 2264 and struct.unpack_from('<4I', packet, 16) ==
                    (1, 18, 10 if step == 3 else 11, 2), 'preserve typed topology')
        common.need(struct.unpack_from('<8I', packet, 320) ==
                    (3 if step == 3 else 2, 80, width, height, width, width, 4, 0x3f800000) and
                    struct.unpack_from('<I', packet, 368)[0] == (76 if step == 3 else 104) and
                    struct.unpack_from('<I', packet, 396)[0] == 1, 'preserve attachment/control contract')
        common.need((fixture/'vertices-source.bin').read_bytes() == vertices and
                    (fixture/'coefficients-source.bin').read_bytes() == (coefficients_a if step == 2 else bytes(b)),
                    'authored normal API inputs')
        placement = (fixture/'placement.bin').read_bytes()
        common.need(len(placement) == 776 and placement[:8] == struct.pack('<II',1,18) and
                    not any(placement[440:]), 'preserve placement capacity')
        bindings = []
        required = {'packet.bin', 'placement.bin', 'vertices-source.bin', 'coefficients-source.bin'}
        for i in range(18):
            va, length, ident, access = struct.unpack_from('<QQII', placement, 8+24*i)
            common.need((ident,access) == struct.unpack_from('<II',packet,32+8*i) and
                        ident and access in (1,3) and va and not va%4096 and length and not length%4096 and
                        va+length <= 2**32 and ident not in [e[2] for e in bindings] and
                        all(not(va < a+n and a < va+length) for a,n,_,_ in bindings), 'preserve binding bounds')
            bindings.append((va,length,ident,access))
            if access == 1:
                name = f'bo-{i}.bin'
                required.add(name)
                common.need(len((fixture/name).read_bytes()) == length, 'READ image extent')
        common.need({p.name for p in fixture.iterdir()} == required, 'typed fixture file inventory')
        color, depth = struct.unpack_from('<II',packet,352)
        common.need(color != depth and all(any(e[2] == ident and e[1] == width*height*4 and e[3] == 3
                    for e in bindings) for ident in (color,depth)), 'owned attachment extents')
        prepared.append((fixture,packet,bindings))
    oracle = root/'oracle'
    metadata = json.loads((oracle/'ORACLE.json').read_text())
    common.need(metadata['classification'] == 'CPU_EXPECTATION_ONLY_NO_GPU' and metadata['size'] == size and
                not metadata['physical_qualification'] and metadata['GPU_operations'] == 0, 'oracle scope')
    regions = (oracle/'REGIONS-u8.bin').read_bytes()
    common.need(len(regions) == width*height and set(regions) == {0,1,2,3}, 'all occlusion regions required')
    colors = [(oracle/name).read_bytes() for name in ('COLOR-B-CLEARED.bin','COLOR-A-ONLY.bin','COLOR-A-THEN-B.bin')]
    z_a = (oracle/'Z24-A-ONLY.bin').read_bytes()
    common.need(all(len(c) == width*height*4 for c in colors+[z_a]), 'oracle extents')
    for region, cb, ca, cab, za in zip(regions, *(struct.iter_unpack('<I', c) for c in colors+[z_a])):
        common.need((cb[0],ca[0],cab[0],za[0]&0xffffff) ==
                    (0xffffffff if region&2 else 0, 0xffffffff if region&1 else 0,
                     0xffffffff if region else 0, 0x800000 if region&1 else 0xffffff), 'sealed region expectations')
    return prepared, colors, z_a, regions


def verify(capture, root, size, manifest_sha, affine_w1=False):
    prepared, colors, z_a, regions = preflight(root,size,manifest_sha,affine_w1)
    common.need(sorted(p.name for p in capture.glob('frame-*')) ==
                [f'frame-{i:06d}' for i in (1,2,3)], 'exact three-draw inventory')
    first_packet = (capture/'frame-000001/packet.bin').read_bytes()
    common.need(len(first_packet) == 2264, 'baseline packet extent')
    depth_id = struct.unpack_from('<I',first_packet,356)[0]
    baseline_z = (capture/'frame-000001'/f'after-id-{depth_id}.bin').read_bytes()
    common.need(len(baseline_z) == len(z_a), 'B depth reference extent')
    far_values = set()
    for (value,),region in zip(struct.iter_unpack('<I',baseline_z),regions):
        value &= 0xffffff
        if region&2:
            common.need(value in (0x9fffff,0xa00000), 'B-only farther depth outside independently bounded quantization candidates')
            far_values.add(value)
        else:
            common.need(value == 0xffffff, 'B-only uncovered Z changed')
    common.need(len(far_values) == 1, 'constant B depth reference required')
    far = next(iter(far_values))
    combined_z = b''.join(struct.pack('<I',0x800000 if region&1 else far if region&2 else 0xffffff) for region in regions)
    frames = []
    for i,(fixture,packet,bindings) in enumerate(prepared,1):
        width,height = map(int,size.split('x'))
        color = colors[i-1]
        gl = b''.join(color[y*width*4:(y+1)*width*4] for y in range(height-1,-1,-1))
        checked = common._verify_frame(capture/f'frame-{i:06d}',fixture,None,size,packet,bindings,
                    {'GL_readback_bottom_up_sha256':sha(gl)},session_ready=True,frame_serial=i,
                    final_ownership=capture,preserve_depth=i==3,expected_color=color,
                    expected_depth=baseline_z if i==1 else z_a if i==2 else combined_z,
                    packet_matcher=_packet_matches if i==3 else None)
        checked['status'] = 'NORMAL_PRESERVE_FRAME_ARCHIVE_MATCHES_SEALED_COLOR_AND_BASELINE_Z'
        common.need(checked['operation'] == (2,4,5)[i-1] and checked['fill_operation'] == (1,3,None)[i-1]
                    and checked['host_operation'] == i, 'exact draw/fill/host operation order')
        if frames:
            common.need(checked['context'] == frames[0]['context'] and checked['attachment_ids'] == frames[0]['attachment_ids']
                        and checked['remaining'] == frames[0]['remaining'], 'same context, attachment generations and guards')
        frames.append(checked)
    prefix = struct.pack('<4IQ',1,0,2,0,frames[0]['context']) + bytes(2264)
    request = (capture/'CLOSE-REQUEST.bin').read_bytes()
    response = (capture/'CLOSE-RESPONSE.bin').read_bytes()
    common.need(request == prefix+bytes(56) and len(response) == 2344 and response[:2288] == prefix and
                struct.unpack_from('<QQ5Ii4I',response,2288) == (0,0,0,0,0,0,0,0,0,8,0,0), 'pure idle source/backing closure')
    common.need((capture/'CLOSE-RESULT.bin').read_bytes() == bytes(4) and
                (capture/'CLOSE-RESPONSE-RESULT.bin').read_bytes() == bytes(4), 'close transport result')
    return dict(status='NORMAL_PRESERVE_THREE_DRAW_ARCHIVE_MATCHES',physical_qualification=False,
                GPU_operations_proven_by_this_tool=0,context=frames[0]['context'],draws=3,fills=2,
                COLOR_mismatches=0,Z24_mismatches_against_references=0,guards_per_frame=4,
                far_Z24_observed_baseline=far,far_Z24_quantizer_conformance_claimed=False,
                source_backing_closed=True,remaining=frames[-1]['remaining'],frames=frames,
                scope='Archive only. Independent boot/module/client/device/source provenance and stock recovery audit remain mandatory.')
