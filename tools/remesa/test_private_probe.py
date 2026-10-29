#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only wire/failure tests. Transport responses are modeled, never GPU evidence."""
import copy
import errno
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import private_probe as p


class Transport:
    def __init__(self, fault=None):
        self.fault = fault
        self.calls = []
        self.objects = {}
        self.probed = False
        self.context = None

    def ioctl(self, fd, number, packet, mutate):
        assert fd == 123 and mutate is True
        if number == p.ioctl_number(9, p.PROBE.size):
            self.probed = True
            self.calls.append(("probe", 0))
            q = list(p.PROBE.unpack(packet))
            assert q[2] == self.context
            q[15:] = [1, 1, 0x01130000, 0x00010201, 9, 7, 0, 0, 15, 0]
            fields = {"cookie": 16, "core": 17, "revision": 18, "phase": 19,
                      "ledger": 20, "retained": 22, "stages": 23, "reserved": 24}
            if self.fault in fields:
                at = fields[self.fault]
                q[at] = q[at] ^ 1
            if self.fault == "immutable": q[3] ^= 1
            if self.fault == "ioctl_error":
                raise OSError(errno.EIO, "modeled uncertain execution")
            packet[:] = p.PROBE.pack(*q)
            return 1 if self.fault == "probe_return" else 0
        assert number == p.ioctl_number(2, p.MEMORY.size)
        q = list(p.MEMORY.unpack(packet))
        version, action, rid, flags, context, size, va, at, n, reserved, data = q
        assert version == 1 and reserved == 0
        if action == p.OPEN:
            assert context == 0
            assert not any(q[i] for i in (2, 3, 5, 6, 7, 8)) and not any(data)
            assert self.context is None
            self.context = 0x123456789abcdef
            q[4] = self.context if self.fault != "open_no_context" else 0
        else:
            assert context == self.context
        self.calls.append((action, rid))
        if action == p.CREATE:
            rid = len(self.objects) + 32
            self.objects[rid] = dict(data=bytearray(size), va=0, pinned=False, lease=0)
            q[2] = rid
        elif action == p.PIN:
            self.objects[rid]["pinned"] = True
        elif action == p.MAP:
            if self.fault == "map_error" and rid == 33:
                raise OSError(errno.ENOMEM, "modeled map failure")
            self.objects[rid]["va"] = va
        elif action == p.LEASE:
            self.objects[rid]["lease"] = flags+1
        elif action == p.UNLEASE:
            self.objects[rid]["lease"] = 0
        elif action == p.WRITE:
            assert self.objects[rid]["lease"] == 2 and n <= 4096
            self.objects[rid]["data"][at:at+n] = data[:n]
        elif action == p.READ:
            assert self.objects[rid]["lease"] == 1 and n <= 4096
            result = bytes(self.objects[rid]["data"][at:at+n])
            if ((self.fault == "input_changed" and rid == 32) or
                (self.fault == "guard_changed" and rid == 43)):
                result = bytes([result[0] ^ 1]) + result[1:]
            q[-1] = result.ljust(4096, b"\0")
        elif action == p.INFO:
            v = [0]*13
            v[3] = len(self.objects)
            if self.fault in ("provider_job", "provider_quarantine"):
                v[7 if self.fault == "provider_job" else 8] = 1
            if rid:
                obj = self.objects[rid]
                v[1:3] = [len(obj["data"]), obj["va"]]
                v[9:13] = [1, len(obj["data"])//4096, 0, len(obj["data"])//4096]
                if self.fault == "mapping": v[12] -= 1
            q[-1] = p.MEMORY_INFO.pack(*v).ljust(4096, b"\0")
        elif action == p.UNMAP:
            if self.fault == "cleanup":
                raise OSError(errno.EBUSY, "modeled cleanup failure")
            self.objects[rid]["va"] = 0
        elif action == p.UNPIN:
            self.objects[rid]["pinned"] = False
        elif action == p.DESTROY:
            del self.objects[rid]
        else:
            assert action in (p.OPEN, p.CLOSE, p.SYNC)
        packet[:] = p.MEMORY.pack(*q)
        return 0


class ProbeClient(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.output = Path(self.tmp.name)
        self.plan = dict(bindings=[dict(role=i, va=0x40001000+i*8192, bytes=4096)
                                   for i in range(11)], guard_va=[0x40000000, 0x40020000])
        self.images = [bytes([i])*4096 for i in range(5)]

    def collect(self, fault=None):
        self.transport = Transport(fault)
        with patch.object(p, "device_identity", return_value={"fixture": "CPU_ONLY"}), \
             patch.object(p.os, "open", return_value=123) as opened, \
             patch.object(p.os, "close") as closed, \
             patch.object(p.fcntl, "ioctl", side_effect=self.transport.ioctl):
            try:
                return p.collect(Path("/never-open-real-device"), self.output,
                                 self.plan, self.images, 10000)
            finally:
                opened.assert_called_once()
                closed.assert_called_once_with(123)

    def test_collection_keeps_oracle_pending_and_reverses_cleanup(self):
        result = self.collect()
        self.assertEqual(result["status"], "RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING")
        self.assertFalse(result["hardware_driver_qualified"])
        self.assertTrue(result["requires_independent_pixel_oracle"])
        self.assertEqual(result["memory_cleanup"], "complete")
        self.assertEqual(self.transport.objects, {})
        frees = [rid for act, rid in self.transport.calls if act == p.DESTROY]
        self.assertEqual(frees, list(reversed(range(32, 45))))

    def test_uncertain_responses_never_send_free_or_close(self):
        for fault in ("cookie", "core", "revision", "phase", "ledger", "retained",
                      "stages", "reserved", "immutable", "ioctl_error",
                      "provider_job", "provider_quarantine", "mapping", "probe_return"):
            with self.subTest(fault=fault):
                with self.assertRaises((ValueError, RuntimeError, OSError)):
                    self.collect(fault)
                report = json.loads((self.output / "RESULT.json").read_text())
                self.assertEqual(report["memory_cleanup"], "suppressed_after_uncertain_probe")
                self.assertTrue((self.output / "RESPONSE.bin").exists())
                self.assertFalse(any(act in (p.UNMAP, p.UNPIN, p.DESTROY, p.CLOSE)
                                     for act, _ in self.transport.calls))

    def test_corruption_after_safe_receipt_is_reported(self):
        for fault in ("input_changed", "guard_changed"):
            with self.subTest(fault=fault):
                with self.assertRaisesRegex(RuntimeError, "changed"):
                    self.collect(fault)
                report = json.loads((self.output / "RESULT.json").read_text())
                self.assertEqual(report["status"], "FAILED")
                self.assertEqual(report["memory_cleanup"], "complete")

    def test_preissue_partial_mapping_failure_unwinds(self):
        with self.assertRaises(OSError): self.collect("map_error")
        self.assertFalse(self.transport.probed)
        self.assertEqual(self.transport.objects, {})

    def test_cleanup_failure_cannot_return_success(self):
        with self.assertRaisesRegex(RuntimeError, "cleanup failed"):
            self.collect("cleanup")
        report = json.loads((self.output / "RESULT.json").read_text())
        self.assertEqual(report["status"], "FAILED")
        self.assertEqual(report["memory_cleanup"], "failed")
        self.assertNotIn((p.CLOSE, 0), self.transport.calls)

    def test_open_requires_server_context_and_never_allocates_without_it(self):
        with self.assertRaisesRegex(RuntimeError, "ownership"):
            self.collect("open_no_context")
        self.assertEqual(self.transport.calls, [(p.OPEN, 0)])
        self.assertFalse(self.transport.probed)
        report = json.loads((self.output / "RESULT.json").read_text())
        self.assertEqual(report["status"], "FAILED")

    def test_bad_memory_envelope_stops_all_subsequent_ioctls(self):
        for field in range(11):
            with self.subTest(field=field):
                m = p.Memory(123, 77)
                def corrupt(fd, number, packet, mutate):
                    q = list(p.MEMORY.unpack(packet))
                    q[field] = bytes([1])+q[field][1:] if field == 10 else q[field]^1
                    packet[:] = p.MEMORY.pack(*q)
                    return 0
                with patch.object(p.fcntl, "ioctl", side_effect=corrupt) as call:
                    with self.assertRaisesRegex(RuntimeError, "ownership"):
                        m.request(p.SYNC, 32)
                    self.assertTrue(m.poisoned)
                    with self.assertRaises(RuntimeError):m.request(p.CLOSE)
                    self.assertEqual(call.call_count, 1)
        with patch.object(p.fcntl, "ioctl", return_value=1):
            with self.assertRaisesRegex(RuntimeError, "ownership"):
                p.Memory(123, 77).request(p.SYNC, 32)

    def test_upload_and_read_use_lease_chunk_bounds_and_sync(self):
        transport = Transport()
        with patch.object(p.fcntl, "ioctl", side_effect=transport.ioctl):
            m = p.Memory(123, 0)
            m.context = m.request(p.OPEN)[4]
            rid = m.request(p.CREATE, size=8192)[2]
            data = bytes(range(256))*32
            m.upload(rid, data)
            self.assertEqual(m.read(rid, len(data)), data)
        self.assertEqual([a for a, _ in transport.calls],
                         [p.OPEN, p.CREATE, p.LEASE, p.WRITE, p.WRITE, p.UNLEASE, p.SYNC,
                          p.LEASE, p.READ, p.READ, p.UNLEASE])

    def test_offline_default_never_opens_device(self):
        destination = self.output / "plan"
        with patch.object(p, "load_assets", return_value=(self.plan, self.images)), \
             patch.object(p.os, "open", side_effect=AssertionError("device open")), \
             patch.object(p, "collect", side_effect=AssertionError("collector")), \
             patch("sys.argv", ["private_probe.py", "--assets", "fixture", "--output", str(destination)]), \
             patch("builtins.print"):
            p.main()
        self.assertEqual(json.loads((destination / "RESULT.json").read_text())["GPU_operations"], 0)

    def test_guard_cannot_overflow_gpu_address_space(self):
        raw = bytearray(5592)
        struct.pack_into("<4I", raw, 0, 0x32505352, 2, 1, 0)
        bindings = copy.deepcopy(self.plan["bindings"])
        bindings[5]["bytes"] = bindings[10]["bytes"] = 4096000
        bindings[5]["va"] = 0x41001000
        bindings[10]["va"] = 0x42000000
        bindings[9]["va"] = 0xfffff000
        for i, b in enumerate(bindings):
            struct.pack_into("<IIQQ", raw, 208+24*i, i+1, 0, b["va"], b["bytes"])
        for i, data in enumerate(self.images):
            (self.output / f"role{i}.bin").write_bytes(data)
            raw[48+32*i:80+32*i] = p.hashlib.sha256(data).digest()
        (self.output / "stream-profile.bin").write_bytes(raw)
        with self.assertRaisesRegex(ValueError, "guard"):
            p.load_assets(self.output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
