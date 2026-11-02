#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Separate exact actual-OpenGL CODE344 LAB catalogue; no device access."""
import argparse
import copy
import json
import struct
from pathlib import Path

from build_gears_scene_probe_profiles import build as build_common, checked
from build_gears_homogeneous_probe_profiles import validate_state15

# Sealed API inputs, not framebuffer outputs. Keep the API's unconsumed alpha
# and normal-w lanes rather than rewriting them to match the older witness.
WITNESS_SHA256 = {
    'code': '53a5b185ee8a0d94de92620f172e8d73fdab15af7267d10639616e93210eadc5',
    'vertices': '1a71886252e21547098f99a96f63721706fe45fb32226cf5b4e4937d0d10609a',
    'coefficient': 'b1e33b2621011483a76877cdd39e5b5aac4b4671193470187fe4380e5ab3834c',
}


def build(c):
    if (c.get('format') != 'REMESA_BOUND_GEARS_CLAMPED_API_V1' or
            c.get('registered_kernel_profiles') is not False):
        raise ValueError('not an unregistered actual-API clamp candidate')
    if [p['proposed_profile'] for p in c['profiles']] != [5, 6]:
        raise ValueError('separate actual-API selectors required')
    for name, digest in WITNESS_SHA256.items():
        if c['witness'][name + '_sha256'] != digest:
            raise ValueError('not the sealed actual-API ' + name)
    normalized = copy.deepcopy(c)
    normalized['format'] = 'REMESA_BOUND_GEARS_POSITIVE_V1'
    text = build_common(normalized, code_bytes=344)
    validate_state15(c)
    for p in c['profiles']:
        packet = checked(p['packet_path'], p['packet_sha256'], 2264)
        images = {b['captured_handle']: checked(b['path'], b['sha256'],
                  b['allocation_bytes']) for b in p['bindings'] if b['access'] == 1}
        uses = [struct.unpack_from('<6I', packet, 400 + 24 * i) for i in range(11)]
        vs = next(u for u in uses if u[2] == 344 and u[4] == 0)
        if vs[1:] != (0, 344, 4, 0, 0):
            raise ValueError('unexpected selected CODE window')
        if images[vs[0]][344:352] != bytes(8):
            raise ValueError('nonzero reserved USE storage')
        if (vs[0], 352, 8, 3, 1, 0) not in uses:
            raise ValueError('fragment body must follow aligned CODE capacity')
    text = text.replace('REMESA_GEARS_SCENE_PROBE_PROFILES_H',
                        'REMESA_GEARS_CLAMPED_API_PROBE_PROFILES_H')
    text = text.replace('"remesa_scene_probe_profiles.h"',
                        '"remesa_gears_scene_probe_profiles.h"')
    text = '\n'.join(line for line in text.split('\n')
                     if not line.startswith('struct remesa_gears_scene_probe_profile {'))
    return (text.replace('gears_scene_probe_packet_', 'gears_clamped_api_probe_packet_')
            .replace(' gears_scene_probe_profiles[2]', ' gears_clamped_api_probe_profiles[2]')
            .replace('tools/remesa/build_gears_scene_probe_profiles.py',
                     'tools/remesa/build_gears_clamped_api_probe_profiles.py'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalogue', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = build(json.loads(args.catalogue.read_text()))
    with args.output.open('x') as output:
        output.write(result)
    print('Generated actual-API CODE344 LAB selectors5/6; no physical approval')


if __name__ == '__main__':
    main()
