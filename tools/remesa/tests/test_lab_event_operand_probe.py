#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Mutator confinement/negative cases only; not a PDS ISA test."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

P = Path(__file__).resolve().parents[1] / 'lab_event_operand_probe.py'
spec = importlib.util.spec_from_file_location('probe', P)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
REF = Path('/home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/stream-profile/role0.bin')
RAW = REF.read_bytes()[0x44c0:0x44c0 + 148]


class ProbeTests(unittest.TestCase):
    def test_all_controls_exact_spans(self):
        self.assertEqual(hashlib.sha256(RAW).hexdigest(), probe.REFERENCE_SHA256)
        for word, ordinal in ((12, 11), (13, 14), (14, 17)):
            for mode in probe.MODES:
                with self.subTest(word=word, mode=mode):
                    source = bytearray(RAW)
                    result, record = probe.build_probe(source, word, mode)
                    self.assertEqual(source, RAW)
                    self.assertEqual(len(result), 148)
                    self.assertEqual(result[60:64], RAW[word * 4:word * 4 + 4])
                    self.assertEqual(result[word * 4:word * 4 + 4], bytes(4) if 'zero_original' in mode else RAW[word * 4:word * 4 + 4])
                    before = struct.unpack('<21I', RAW[64:])
                    after = struct.unpack('<21I', result[64:])
                    for i in range(21):
                        if i == ordinal and mode.startswith('select_'):
                            self.assertEqual(after[i] >> 10 & 7, 7)
                            self.assertEqual((after[i] ^ before[i]) & ~0x1c00, 0)
                        else:
                            self.assertEqual(after[i], before[i])
                    self.assertEqual(record['changed_bytes'], [i for i in range(148) if result[i] != RAW[i]])
                    self.assertEqual(record['CODE_bytes_added'], 0)
                    self.assertFalse(record['physical_approval'])
                    self.assertEqual(result[:48], RAW[:48])  # All three relocation sites intact.
                    for cell in (12, 13, 14):
                        expected = bytes(4) if cell == word and 'zero_original' in mode else RAW[cell * 4:cell * 4 + 4]
                        self.assertEqual(result[cell * 4:cell * 4 + 4], expected)

    def test_reject_every_noncanonical_preimage(self):
        for i in range(148):
            bad = bytearray(RAW)
            bad[i] ^= 1
            with self.subTest(byte=i), self.assertRaises(ValueError):
                probe.build_probe(bad, 12, 'copy_only')
        for bad in (None, b'', RAW[:-1], RAW + b'\0', 'text'):
            with self.assertRaises(ValueError):
                probe.build_probe(bad, 12, 'copy_only')

    def test_reject_unknown_requests(self):
        for word in (-1, 0, 11, 15, 16, 0xffffffff, True, 12.0, '12'):
            with self.assertRaises(ValueError):
                probe.build_probe(RAW, word, 'copy_only')
        for mode in ('', 'execute', 'SELECT_COPY', None):
            with self.assertRaises(ValueError):
                probe.build_probe(RAW, 12, mode)

    def test_cli_not_launchable_and_no_overwrite(self):
        with tempfile.TemporaryDirectory(prefix='remesa-event-probe-test-') as t:
            root = Path(t)
            ref = root / 'event.bin'
            ref.write_bytes(RAW)
            output = root / 'probes'
            cmd = [sys.executable, '-B', str(P), '--reference-event', str(ref), '--output', str(output)]
            first = subprocess.run(cmd, capture_output=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            manifest = json.loads((output / 'MANIFEST.json').read_text())
            self.assertEqual(len(manifest['variants']), 12)
            self.assertEqual(manifest['GPU_operations'], 0)
            self.assertFalse(manifest['physical_approval'])
            self.assertFalse(manifest['native_admission'])
            self.assertFalse(manifest['instruction_encoder'])
            self.assertEqual(manifest['status'], 'LAB_OPERAND_HYPOTHESES_ONLY_NOT_LAUNCHABLE')
            saved = {p.name: p.read_bytes() for p in output.iterdir()}
            second = subprocess.run(cmd, capture_output=True)
            self.assertNotEqual(second.returncode, 0)
            self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, saved)
            self.assertEqual(ref.read_bytes(), RAW)


if __name__ == '__main__':
    unittest.main()
