#!/usr/bin/env python3
"""Collect one private-probe receipt/readback. Defaults to an offline plan.

The current driver's PCI removal/quarantine integration is not qualified. This
client does not enable module parameters, install modules, reboot or present a
frame. Physical use requires completing that driver qualification first.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import time

MEMORY = struct.Struct("<4I4Q2I4096s")
PROBE = struct.Struct("<IIQ11IIQQ4Ii3I")
MEMORY_INFO = struct.Struct("<3Q10I")
CREATE, PIN, MAP, UNMAP, UNPIN, DESTROY, LEASE, READ, WRITE, UNLEASE, SYNC, OPEN, CLOSE, INFO = range(1, 15)
assert MEMORY.size == 4152 and PROBE.size == 112


def ioctl_number(index, size):
    return (3 << 30) | (size << 16) | (ord("d") << 8) | (0x40 + index)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_assets(directory):
    raw = (directory / "stream-profile.bin").read_bytes()
    if len(raw) != 5592 or struct.unpack_from("<2I", raw) != (0x32505352, 2):
        raise ValueError("unexpected retained profile format")
    count, reserved = struct.unpack_from("<2I", raw, 8)
    if not 0 < count <= 64 or reserved:
        raise ValueError("invalid profile batch count/reserved")
    bindings = []
    for i in range(11):
        ignored_id, unused, va, length = struct.unpack_from("<IIQQ", raw, 208 + 24 * i)
        if unused or not length or va & 4095 or length & 4095 or va + length > 2**32:
            raise ValueError("invalid retained binding")
        bindings.append(dict(role=i, va=va, bytes=length))
    images = [(directory / f"role{i}.bin").read_bytes() for i in range(5)]
    for i, data in enumerate(images):
        if len(data) != bindings[i]["bytes"] or hashlib.sha256(data).digest() != raw[48 + 32*i:80 + 32*i]:
            raise ValueError(f"role{i} does not match retained full-image hash")
    if bindings[5]["bytes"] != 4096000 or bindings[10]["bytes"] != 4096000:
        raise ValueError("this probe requires the retained 1280x800 profile")
    guards = [bindings[5]["va"]-4096, bindings[9]["va"]+bindings[9]["bytes"]]
    for va in guards:
        if va < 0 or va+4096 > 2**32 or any(va < b["va"]+b["bytes"] and b["va"] < va+4096 for b in bindings):
            raise ValueError("guard page overlaps a retained resource")
    return dict(profile_sha256=sha(raw), bindings=bindings, guard_va=guards,
                input_sha256=[sha(x) for x in images]), images


class Memory:
    def __init__(self, fd, context):
        self.fd, self.context = fd, context

    def request(self, action, object_id=0, flags=0, size=0, va=0, offset=0, length=0, data=b""):
        if len(data) > 4096:
            raise ValueError("memory chunk exceeds ABI")
        packet = bytearray(MEMORY.pack(1, action, object_id, flags, self.context,
                                      size, va, offset, length, 0, data.ljust(4096, b"\0")))
        fcntl.ioctl(self.fd, ioctl_number(2, MEMORY.size), packet, True)
        return MEMORY.unpack(packet)

    def upload(self, object_id, data):
        self.request(LEASE, object_id, flags=1)
        try:
            for at in range(0, len(data), 4096):
                chunk = data[at:at+4096]
                self.request(WRITE, object_id, offset=at, length=len(chunk), data=chunk)
        finally:
            self.request(UNLEASE, object_id)
        self.request(SYNC, object_id)

    def read(self, object_id, length):
        self.request(LEASE, object_id)
        result = bytearray()
        try:
            for at in range(0, length, 4096):
                n = min(4096, length-at)
                result.extend(self.request(READ, object_id, offset=at, length=n)[-1][:n])
        finally:
            self.request(UNLEASE, object_id)
        return bytes(result)

    def info(self, object_id=0):
        return MEMORY_INFO.unpack(self.request(INFO, object_id)[-1][:MEMORY_INFO.size])


def receipt(packet, context, ids, budget):
    values = PROBE.unpack(packet)
    if list(values[:15]) != [1, 0, context, *ids, budget] or values[24]:
        raise ValueError("probe response changed immutable request fields")
    result = dict(zip(("operation", "cookie", "core", "revision", "phase", "ledger",
                       "error", "retained", "stages"), values[15:24]))
    if (result["error"] or result["retained"] or result["stages"] != 15 or
        result["core"] != 0x01130000 or result["revision"] != 0x00010201 or
        result["phase"] != 9 or result["ledger"] != 7 or not result["operation"] or
        result["cookie"] != result["operation"]):
        raise RuntimeError("private probe did not return a complete released receipt: " + json.dumps(result))
    return result


def device_identity(path):
    node = path.stat()
    if not stat.S_ISCHR(node.st_mode):
        raise ValueError("device must be a DRM character device")
    base = Path(f"/sys/dev/char/{os.major(node.st_rdev)}:{os.minor(node.st_rdev)}/device")
    result = {name: (base / name).read_text().strip() for name in ("vendor", "device", "revision")}
    if (result["vendor"], result["device"], result["revision"]) != ("0x8086", "0x8108", "0x06"):
        raise ValueError("unexpected Dell Mini12 PCI identity")
    return result


def collect(device, output, plan, images, budget):
    identity = device_identity(device)
    report = dict(status="PREPARING", identity=identity, plan=plan,
                  probe_ioctl_calls=0, hardware_driver_qualified=False,
                  requires_independent_pixel_oracle=True)
    def save():
        (output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n")
    save()
    fd = os.open(device, os.O_RDWR | os.O_CLOEXEC)
    memory = Memory(fd, (time.time_ns() ^ os.getpid()) & ((1 << 64)-1) or 1)
    objects, opened, probe_attempted, probe_complete = [], False, False, False
    try:
        memory.request(OPEN);opened = True
        start = time.monotonic()
        for binding in plan["bindings"]:
            object_id = memory.request(CREATE, size=binding["bytes"])[2]
            # Track partial construction so pre-issue failure can unwind safely.
            state = dict(id=object_id, pinned=False, mapped=False)
            objects.append(state)
            memory.request(PIN, object_id);state["pinned"] = True
            memory.request(MAP, object_id, va=binding["va"]);state["mapped"] = True
            if binding["role"] < 5:
                memory.upload(object_id, images[binding["role"]])
        guards = []
        for va in plan["guard_va"]:
            object_id = memory.request(CREATE, size=4096)[2]
            state = dict(id=object_id, pinned=False, mapped=False)
            objects.append(state)
            memory.request(PIN, object_id);state["pinned"] = True
            memory.request(MAP, object_id, va=va);state["mapped"] = True
            memory.upload(object_id, b"\xa5" * 4096)
            guards.append(dict(id=object_id, va=va))
        report["prepare_and_upload_wall_seconds"] = time.monotonic()-start
        ids = [x["id"] for x in objects[:11]]
        packet = bytearray(PROBE.pack(1, 0, memory.context, *ids, budget, *([0]*10)))
        (output / "REQUEST.bin").write_bytes(packet)
        probe_attempted = True;report["probe_ioctl_calls"] = 1;save()
        start = time.monotonic()
        try:
            fcntl.ioctl(fd, ioctl_number(9, PROBE.size), packet, True)
        finally:
            (output / "RESPONSE.bin").write_bytes(packet)
            report["probe_ioctl_wall_seconds"] = time.monotonic()-start
        report["receipt"] = receipt(packet, memory.context, ids, budget)
        info = memory.info()
        if any(info[i] for i in (6, 7, 8)):
            raise RuntimeError("provider retains leases/jobs/quarantine after receipt")
        report["provider_info"] = list(info)
        expected = [(obj["id"], binding["va"], binding["bytes"])
                    for obj, binding in zip(objects[:11], plan["bindings"])]
        expected += [(g["id"], g["va"], 4096) for g in guards]
        for object_id, va, length in expected:
            info = memory.info(object_id)
            if (info[1:3] != (length, va) or any(info[i] for i in (6, 7, 8, 11)) or
                info[9] != 1 or info[10] != length//4096 or info[12] != length//4096):
                raise RuntimeError("restored provider mapping/ownership mismatch")
        probe_complete = True
        start = time.monotonic();readbacks = {}
        for role in (0, 1, 2, 3, 4, 5, 10):
            data = memory.read(ids[role], plan["bindings"][role]["bytes"])
            (output / f"role{role}-readback.bin").write_bytes(data)
            readbacks[str(role)] = dict(bytes=len(data), sha256=sha(data))
            if role < 5 and data != images[role]:
                raise RuntimeError(f"readonly/LOCAL input role{role} changed")
        report["readback_wall_seconds"] = time.monotonic()-start
        report["readbacks"] = readbacks
        for i, guard in enumerate(guards):
            data = memory.read(guard["id"], 4096)
            (output / f"guard{i}-readback.bin").write_bytes(data)
            if data != b"\xa5" * 4096:
                raise RuntimeError("guard page changed")
        report["guards_intact"] = True
        report["status"] = "RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING"
    except BaseException as error:
        report["status"] = "FAILED";report["error"] = str(error)
        raise
    finally:
        # Never send unmap/free/close ioctls after an uncertain probe response.
        # Closing the OS FD delegates retained-state handling to the driver.
        if opened and (not probe_attempted or probe_complete):
            try:
                for obj in reversed(objects):
                    if obj["mapped"]:memory.request(UNMAP, obj["id"])
                    if obj["pinned"]:memory.request(UNPIN, obj["id"])
                    memory.request(DESTROY, obj["id"])
                memory.request(CLOSE)
                report["memory_cleanup"] = "complete"
            except Exception as error:
                report["memory_cleanup"] = "failed";report["cleanup_error"] = str(error)
                report["collection_status"] = report["status"]
                report["status"] = "FAILED"
        else:
            report["memory_cleanup"] = "suppressed_after_uncertain_probe"
        try:
            os.close(fd)
        finally:
            save()
    if report.get("memory_cleanup") == "failed":
        raise RuntimeError("private probe cleanup failed: " + report["cleanup_error"])
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--assets", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--device", type=Path)
    p.add_argument("--execute", action="store_true")
    p.add_argument("--budget-ms", type=int, default=10000)
    a = p.parse_args()
    if not 1 <= a.budget_ms <= 10000:p.error("budget must be 1..10000 ms")
    if a.execute and not a.device:p.error("--execute requires --device")
    plan, images = load_assets(a.assets)
    a.output.mkdir(parents=True, exist_ok=False)
    if a.execute:
        report = collect(a.device, a.output, plan, images, a.budget_ms)
    else:
        report = dict(status="OFFLINE_PLAN_ONLY", GPU_operations=0, plan=plan,
                      hardware_driver_qualified=False)
        (a.output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
