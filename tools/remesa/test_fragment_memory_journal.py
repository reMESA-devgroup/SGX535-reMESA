#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run against --capture-directory from the interposed production draw test.
All test evidence is CPU-only. No device, rendering or completion is simulated
by the reader. The input fixture producer labels its own synthetic observations.
"""
import argparse
import copy
import json
from pathlib import Path
import struct
import unittest
from fragment_memory_journal import reconstruct, packet, RECORD_BYTES, PACKET_BYTES


class Journal(unittest.TestCase):
    def setUp(self):
        self.m = copy.deepcopy(self.markers)

    def post_read(self, rid):
        for i in range(self.m['submit_end'], self.m['capture_end'], 2):
            at = i*RECORD_BYTES+16
            q, _ = packet(self.raw[at:at+PACKET_BYTES])
            if q['id'] == rid and q['action'] == 8:
                return i
        self.fail('fixture missing expected actual MEM_READ')

    def test_complete_actual_records(self):
        r = reconstruct(self.raw, self.m)
        self.assertEqual(r['classification'], 'JOURNAL_RECONSTRUCTION_ONLY')
        self.assertEqual(r['memory']['after'][str(self.m['roles'][5]['id'])]['data'], '7b'*4096)
        self.assertEqual(sum(v['bytes'] for b in r['memory'].values() for v in b.values()), 376832)

    def test_reference_cannot_supply_observed_pixels(self):
        r = reconstruct(self.raw, self.m)
        self.assertNotIn('expected_pixels', r)
        self.assertEqual(bytes.fromhex(r['memory']['after'][str(self.m['roles'][5]['id'])]['data']), b'\x7b'*4096)

    def test_truncated(self):
        with self.assertRaises(ValueError): reconstruct(self.raw[:-1], self.m)

    def test_unsealed_tail(self):
        with self.assertRaises(ValueError): reconstruct(self.raw+self.raw[-RECORD_BYTES*2:], self.m)

    def test_missing_page_is_not_filled_from_writes(self):
        i = self.post_read(self.m['roles'][0]['id'])
        raw = self.raw[:i*RECORD_BYTES]+self.raw[(i+2)*RECORD_BYTES:]
        self.m['capture_end'] -= 2
        with self.assertRaisesRegex(ValueError, 'incomplete observed coverage'):
            reconstruct(raw, self.m)

    def duplicate(self, corrupt):
        i = self.post_read(self.m['roles'][0]['id'])
        pair = bytearray(self.raw[i*RECORD_BYTES:(i+2)*RECORD_BYTES])
        if corrupt: pair[RECORD_BYTES+16+PACKET_BYTES+56] ^= 1
        raw = self.raw[:(i+2)*RECORD_BYTES]+pair+self.raw[(i+2)*RECORD_BYTES:]
        self.m['capture_end'] += 2
        return raw

    def test_identical_overlapping_reads(self):
        reconstruct(self.duplicate(False), self.m)

    def test_contradictory_overlapping_reads(self):
        with self.assertRaisesRegex(ValueError, 'contradictory overlapping read'):
            reconstruct(self.duplicate(True), self.m)

    def test_foreign_session(self):
        raw = bytearray(self.raw);i = self.post_read(self.m['roles'][0]['id'])
        for at in (i*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16+PACKET_BYTES):
            struct.pack_into('<Q', raw, at+16, self.m['capture']['owner']+1)
        with self.assertRaisesRegex(ValueError, 'foreign context'): reconstruct(bytes(raw), self.m)

    def test_stale_backing(self):
        self.m['capture']['program_handle'] += 100
        with self.assertRaises(ValueError): reconstruct(self.raw, self.m)

    def test_invalid_bounds(self):
        self.m['capture']['source_offset'] = self.m['capture']['program_bytes']
        with self.assertRaises(ValueError): reconstruct(self.raw, self.m)

    def test_alias(self):
        self.m['capture']['geometry_handle'] = self.m['roles'][0]['id']
        with self.assertRaises(ValueError): reconstruct(self.raw, self.m)

    def test_va_overlap(self):
        self.m['roles'][1]['va'] = self.m['roles'][0]['va']
        with self.assertRaises(ValueError): reconstruct(self.raw, self.m)

    def test_malformed_transfer(self):
        raw = bytearray(self.raw);i = self.post_read(self.m['roles'][5]['id'])
        for at in (i*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16+PACKET_BYTES):
            struct.pack_into('<Q', raw, at+40, 4096)
        with self.assertRaisesRegex(ValueError, 'transfer bounds'): reconstruct(bytes(raw), self.m)

    def test_cpu_mutation_after_submit(self):
        raw = bytearray(self.raw);i = self.m['submit_end']
        for at in (i*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16, (i+1)*RECORD_BYTES+16+PACKET_BYTES):
            self.assertEqual(struct.unpack_from('<I', raw, at+4)[0], 7)
            struct.pack_into('<I', raw, at+12, 1)
        with self.assertRaisesRegex(ValueError, 'CPU mutation'): reconstruct(bytes(raw), self.m)

    def test_forged_origin_label(self):
        self.m['scope'] = 'CONFIRMED_ON_HARDWARE'
        with self.assertRaises(ValueError): reconstruct(self.raw, self.m)

    def test_failed_syscall(self):
        raw = bytearray(self.raw);struct.pack_into('<i', raw, RECORD_BYTES+4, -1)
        with self.assertRaises(ValueError): reconstruct(bytes(raw), self.m)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture-directory', type=Path, required=True)
    a = p.parse_args()
    Journal.raw = (a.capture_directory/'memory.bin').read_bytes()
    Journal.markers = json.loads((a.capture_directory/'capture.json').read_text())
    unittest.main(argv=[__file__], verbosity=2)
