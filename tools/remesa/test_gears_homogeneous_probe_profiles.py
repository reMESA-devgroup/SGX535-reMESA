#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Authenticate sealed inputs and reject altered state15 catalogues; GPU0."""
import argparse
import copy
import hashlib
import json
import struct
import tempfile
from pathlib import Path

from build_gears_homogeneous_probe_profiles import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalogue', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    args = parser.parse_args()
    catalogue = json.loads(args.catalogue.read_text())
    assert build(catalogue) == args.header.read_text()
    rejected = []

    def refuse(name, candidate):
        try:
            build(candidate)
        except ValueError:
            rejected.append(name)
        else:
            raise AssertionError('accepted ' + name)

    for field, value in [('registered_kernel_profiles', True),
                         ('physical_qualification', True), ('GPU_operations', 1)]:
        candidate = copy.deepcopy(catalogue)
        candidate[field] = value
        refuse(field, candidate)
    for index in range(2):
        candidate = copy.deepcopy(catalogue)
        candidate['profiles'][index]['proposed_profile'] = index + 1
        refuse('old selector ' + str(index), candidate)
    for field in ['code_sha256', 'vertices_sha256', 'coefficient_sha256']:
        candidate = copy.deepcopy(catalogue)
        candidate['witness'][field] = '0' * 64
        refuse(field, candidate)

    # Re-authenticate deliberately changed images so these exercise semantic
    # checks, rather than merely rejecting an obsolete image digest.
    with tempfile.TemporaryDirectory(prefix='remesa-state15-') as temporary:
        for index in range(2):
            profile = catalogue['profiles'][index]
            state = next(b for b in profile['bindings'] if b['access'] == 1
                         and Path(b['path']).read_bytes()[:4] == struct.pack('<I', 0x1f41))
            for label, offset in [('header', 0), ('viewport', 20), ('mode', 56),
                                  ('padding', 60), ('MOVE', 64), ('tail', 76),
                                  ('PDS address', 96), ('PDS count', 100)]:
                candidate = copy.deepcopy(catalogue)
                binding = next(b for b in candidate['profiles'][index]['bindings']
                               if b['captured_handle'] == state['captured_handle'])
                data = bytearray(Path(binding['path']).read_bytes())
                data[offset] ^= 1
                image = Path(temporary) / 'state.bin'
                image.write_bytes(data)
                binding['path'] = str(image)
                binding['sha256'] = hashlib.sha256(data).hexdigest()
                refuse(str(index) + ' ' + label, candidate)
    print('PASS deterministic separate catalogue; ' + str(len(rejected)) +
          ' altered metadata/state contracts refused; GPU0')


if __name__ == '__main__':
    main()
