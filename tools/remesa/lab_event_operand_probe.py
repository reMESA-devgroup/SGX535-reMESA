#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Strict LAB-only edits of a pinned historical event oracle, NOT an encoder.

Hypothesis only: DATA words12/13/14 correspond to selector bits12:10 of
CODE words11/14/17. Word15 was unwritten by the CPU producer; it is NOT
assumed dead. Four controls per source are necessary to distinguish effects.
No ioctl, boot, launch, admission, MMIO or physical approval exists here.
No executable template is embedded in this tool or added to the Mesa driver.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

EVENT_BYTES = 148
DATA_BYTES = 64
CODE_BYTES = 84
REFERENCE_SHA256 = '8ec9cecfde60a0312c5ae83ba7ffae34d45fd26bbaad7628e8afee5867a15061'
SITES = {12: 11, 13: 14, 14: 17}
MODES = ('copy_only', 'select_copy', 'copy_zero_original', 'select_copy_zero_original')
SELECTOR_MASK = 7 << 10
COPY_WORD = 15


def digest(data):
    return hashlib.sha256(data).hexdigest()


def reference_bytes(data):
    if not isinstance(data, (bytes, bytearray)) or len(data) != EVENT_BYTES:
        raise ValueError('Expected exactly148 reference bytes')
    raw = bytes(data)
    if digest(raw) != REFERENCE_SHA256:
        raise ValueError('Reference is not the pinned historical event oracle')
    return raw


def build_probe(reference, source_word, mode):
    raw = reference_bytes(reference)
    if type(source_word) is not int or source_word not in SITES:
        raise ValueError('Only DATA source words12/13/14 are hypotheses')
    if mode not in MODES:
        raise ValueError('Unknown probe control mode')
    code_word = SITES[source_word]
    code_offset = DATA_BYTES + 4 * code_word
    original = struct.unpack_from('<I', raw, code_offset)[0]
    # Numerical candidate constraint only; no architectural bank/opcode claim.
    if (original & SELECTOR_MASK) >> 10 != source_word - 8:
        raise ValueError('Unexpected historical selector pattern')
    if struct.unpack_from('<I', raw, COPY_WORD * 4)[0] != 0:
        raise ValueError('Unexpected historical unwritten-cell snapshot')
    image = bytearray(raw)
    image[4 * COPY_WORD:4 * COPY_WORD + 4] = raw[4 * source_word:4 * source_word + 4]
    select = mode in ('select_copy', 'select_copy_zero_original')
    zero = mode in ('copy_zero_original', 'select_copy_zero_original')
    if select:
        struct.pack_into('<I', image, code_offset, (original & ~SELECTOR_MASK) | (7 << 10))
    if zero:
        image[4 * source_word:4 * source_word + 4] = bytes(4)
    allowed = set(range(4 * COPY_WORD, 4 * COPY_WORD + 4))
    if zero:
        allowed.update(range(4 * source_word, 4 * source_word + 4))
    if select:
        allowed.update(range(code_offset, code_offset + 4))
    changed = [i for i, (a, b) in enumerate(zip(raw, image)) if a != b]
    if not changed or not set(changed) <= allowed or len(image) != EVENT_BYTES:
        raise ValueError('Mutation escaped the exact probe span')
    patched = struct.unpack_from('<I', image, code_offset)[0]
    if (patched ^ original) & ~SELECTOR_MASK:
        raise ValueError('CODE control bits changed outside selector hypothesis')
    return bytes(image), {
        'source_DATA_word': source_word, 'copy_DATA_word': COPY_WORD,
        'CODE_word': code_word, 'selector_mask': hex(SELECTOR_MASK),
        'mode': mode, 'changed_bytes': changed,
        'CODE_bytes_added': 0, 'event_bytes': EVENT_BYTES,
        'DATA_bytes': DATA_BYTES, 'CODE_bytes': CODE_BYTES,
        'sha256': digest(image), 'physical_approval': False,
        'scope': 'Candidate bit-field edit only; not an instruction encoder or memory-safety proof',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-event', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    raw = reference_bytes(args.reference_event.read_bytes())
    variants = [(word, mode, *build_probe(raw, word, mode)) for word in SITES for mode in MODES]
    # Refuse overwriting any existing evidence directory.
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'reference.bin').write_bytes(raw)
    rows = []
    for word, mode, image, row in variants:
        filename = f'data{word}-{mode}.bin'
        (args.output / filename).write_bytes(image)
        row['file'] = filename
        rows.append(row)
    manifest = {
        'status': 'LAB_OPERAND_HYPOTHESES_ONLY_NOT_LAUNCHABLE',
        'reference_sha256': REFERENCE_SHA256, 'GPU_operations': 0,
        'physical_approval': False, 'native_admission': False,
        'instruction_encoder': False, 'copy_cell_not_assumed_dead': True,
        'variants': rows,
        'required_before_physical': [
            'Strict separate LAB profile/gates, complete address/PFN/permission isolation and failure handling',
            'Recoverable N/N boot and rollback with no pending operations',
            'Required paired1280x800/640x640 validation; occasional641x479, APIs reported separately',
            'Identity/submission/completion/retirement/readback and integrity evidence for each test',
        ],
        'interpretation': 'Only controlled GPU observations can support or refute the selector hypothesis; matching bytes alone do not approve an encoder',
    }
    (args.output / 'MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Generated12 LAB hypothesis files; GPU0; no launch/admission/encoder approval')


if __name__ == '__main__':
    main()
