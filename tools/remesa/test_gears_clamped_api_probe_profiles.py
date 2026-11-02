#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exact API input catalogue regressions, including rehashed corruptions; GPU0."""
import argparse
import copy
import hashlib
import json
import struct
import tempfile
from pathlib import Path

from build_gears_clamped_api_probe_profiles import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalogue', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    args = parser.parse_args()
    c = json.loads(args.catalogue.read_text())
    assert build(c) == args.header.read_text()
    rejected = []

    def refuse(name, candidate):
        try:
            build(candidate)
        except ValueError:
            rejected.append(name)
        else:
            raise AssertionError('accepted ' + name)

    for field, value in [('format', 'REMESA_BOUND_GEARS_HOMOGENEOUS_V1'),
                         ('registered_kernel_profiles', True),
                         ('physical_qualification', True), ('GPU_operations', 1)]:
        candidate = copy.deepcopy(c)
        candidate[field] = value
        refuse(field, candidate)
    for name in ['code', 'vertices', 'coefficient']:
        candidate = copy.deepcopy(c)
        candidate['witness'][name + '_sha256'] = '0' * 64
        refuse(name + ' witness', candidate)
    with tempfile.TemporaryDirectory(prefix='remesa-api-catalogue-') as temporary:
        image = Path(temporary) / 'image.bin'
        for index, profile in enumerate(c['profiles']):
            candidate = copy.deepcopy(c)
            candidate['profiles'][index]['proposed_profile'] -= 2
            refuse(str(index) + ' legacy selector', candidate)
            packet = Path(profile['packet_path']).read_bytes()
            uses = [struct.unpack_from('<6I', packet, 400 + 24 * i) for i in range(11)]
            vs = next(u for u in uses if u[2] == 344)
            state = next(b for b in profile['bindings'] if b['access'] == 1
                         and Path(b['path']).read_bytes()[:4] == struct.pack('<I', 0x1f41))
            faults = [(vs[0], at, label) for at, label in
                      [(0, 'body'), (312, 'clamp'), (336, 'tail'), (344, 'capacity padding')]]
            faults += [(state['captured_handle'], at, label) for at, label in
                       [(0, 'state14'), (20, 'viewport'), (56, 'mode'), (60, 'state padding'),
                        (64, 'MOVE'), (76, 'tail'), (96, 'PDS address'), (100, 'PDS count')]]
            for rid, offset, label in faults:
                candidate = copy.deepcopy(c)
                binding = next(b for b in candidate['profiles'][index]['bindings']
                               if b['captured_handle'] == rid)
                data = bytearray(Path(binding['path']).read_bytes())
                data[offset] ^= 1
                image.write_bytes(data)
                binding['path'] = str(image)
                binding['sha256'] = hashlib.sha256(data).hexdigest()
                refuse(str(index) + ' rehashed ' + label, candidate)
            for at, label in [(400 + 24 * uses.index(vs) + 8, 'declared CODE'),
                              (400 + 24 * uses.index((vs[0], 352, 8, 3, 1, 0)) + 4, 'FS offset')]:
                candidate = copy.deepcopy(c)
                data = bytearray(packet)
                data[at] ^= 1
                image.write_bytes(data)
                candidate['profiles'][index]['packet_path'] = str(image)
                candidate['profiles'][index]['packet_sha256'] = hashlib.sha256(data).hexdigest()
                refuse(str(index) + ' rehashed ' + label, candidate)
    print('PASS deterministic actual-API catalogue; ' + str(len(rejected)) +
          ' altered metadata/CODE/state/capacity contracts refused; GPU0')


if __name__ == '__main__':
    main()
